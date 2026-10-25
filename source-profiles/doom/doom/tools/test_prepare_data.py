#!/usr/bin/env python3
"""Offline tests; no network requests, private keys or device connections.

Run: python3 -B -m unittest discover -s local/pxa-apps/doom/tools -p test_prepare_data.py -v
Generated fixtures live under local/artifacts/doom/<timestamp>/ and are cleaned up.
"""
import contextlib
from datetime import datetime
import hashlib
import importlib.util
import io
import json
from pathlib import Path
import struct
import tempfile
from types import SimpleNamespace
import unittest
from unittest import mock

TOOLS = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location('doom_prepare_data', TOOLS / 'prepare-data.py')
prepare = importlib.util.module_from_spec(spec)
spec.loader.exec_module(prepare)
REPOSITORY = TOOLS.parents[3]
ARTIFACTS = REPOSITORY / 'local/artifacts/doom' / datetime.now().strftime('%Y%m%d-%H%M%S')


def make_wad(map_name=b'E1M1', missing=None):
    names = [b'PLAYPAL', b'COLORMAP', b'TITLEPIC', map_name]
    names = [name for name in names if name != missing]
    body = bytearray(b'IWAD' + b'\0' * 8)
    entries = []
    for index, name in enumerate(names):
        payload = b'' if name == map_name else bytes([index + 1])
        entries.append((len(body) if payload else 0, len(payload), name))
        body.extend(payload)
    directory = len(body)
    for offset, size, name in entries:
        body.extend(struct.pack('<ii8s', offset, size, name))
    struct.pack_into('<ii', body, 4, len(entries), directory)
    return bytes(body)


def patch_int(data, offset, value):
    result = bytearray(data)
    struct.pack_into('<i', result, offset, value)
    return bytes(result)


def frame(kind, payload):
    return SimpleNamespace(kind=kind, payload=payload)


class ArtifactTest(unittest.TestCase):
    def setUp(self):
        ARTIFACTS.mkdir(parents=True, exist_ok=True)
        temporary = tempfile.TemporaryDirectory(prefix='prepare-data-', dir=ARTIFACTS)
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        self.output = self.root / 'media/files'
        self.data = make_wad()

    def source(self, name='doom2.wad', data=None):
        path = self.root / name
        path.write_bytes(self.data if data is None else data)
        return path


class ValidationTests(ArtifactTest):
    def test_both_map_styles_and_zero_length_marker(self):
        for map_name in (b'E1M1', b'MAP01'):
            with self.subTest(map_name=map_name):
                data = make_wad(map_name)
                result = prepare.validate_wad(data)
                self.assertEqual(result, {'sha256': hashlib.sha256(data).hexdigest(),
                                          'bytes': len(data), 'lumps': 4})

    def test_invalid_header_and_directory_bounds(self):
        cases = [b'', self.data[:11], b'PWAD' + self.data[4:],
                 patch_int(self.data, 4, -1), patch_int(self.data, 4, 0),
                 patch_int(self.data, 4, 2**31 - 1),
                 patch_int(self.data, 8, -1), patch_int(self.data, 8, 0),
                 patch_int(self.data, 8, len(self.data) + 1), self.data[:-1]]
        for data in cases:
            with self.subTest(data=data[:12]):
                with self.assertRaises(ValueError):
                    prepare.validate_wad(data)

    def test_each_lump_boundary_is_checked(self):
        directory = struct.unpack_from('<i', self.data, 8)[0]
        for offset, value in ((directory, -1), (directory, len(self.data) + 1),
                              (directory, 0), (directory + 4, -1),
                              (directory + 4, len(self.data)),
                              (directory + 3 * 16, len(self.data) + 1)):
            with self.subTest(offset=offset, value=value):
                with self.assertRaises(ValueError):
                    prepare.validate_wad(patch_int(self.data, offset, value))

    def test_required_lumps(self):
        for name in (b'PLAYPAL', b'COLORMAP', b'TITLEPIC', b'E1M1'):
            with self.subTest(name=name):
                with self.assertRaisesRegex(ValueError, 'missing|required|E1M1'):
                    prepare.validate_wad(make_wad(missing=name))

    def test_twenty_mib_inclusive_and_oversize(self):
        data = self.data.ljust(prepare.MAX_WAD_BYTES, b'\0')
        self.assertEqual(prepare.validate_wad(data)['bytes'], prepare.MAX_WAD_BYTES)
        with self.assertRaises(ValueError):
            prepare.validate_wad(data + b'\0')
        large = self.root / 'doom.wad'
        with large.open('wb') as stream:
            stream.truncate(prepare.MAX_WAD_BYTES + 1)
        with self.assertRaisesRegex(ValueError, '20 MiB'):
            prepare.read_wad(large)

    def test_actual_pinned_upstream_when_available(self):
        upstream = REPOSITORY / 'local/upstream-puredoom/doom1.wad'
        if not upstream.is_file():
            self.skipTest('local upstream checkout unavailable')
        data, metadata = prepare.read_wad(upstream)
        self.assertEqual(upstream.stat().st_size, 4196020)
        self.assertEqual(metadata['bytes'], prepare.SHAREWARE_BYTES)
        self.assertEqual(metadata['sha256'], prepare.SHAREWARE_SHA256)
        self.assertGreater(metadata['lumps'], 4)
        manifest = prepare.prepare(self.output, upstream=upstream.parent)
        self.assertEqual((self.output / 'doom1.wad').read_bytes(), data)
        self.assertEqual(manifest['upstream_commit'], prepare.COMMIT)


class PreparationTests(ArtifactTest):
    def test_default_output_is_sibling_media_directory(self):
        self.assertEqual(prepare.DEFAULT_OUTPUT, prepare.APP_ROOT.parent / '.game-data/doom/files')

    def test_all_supported_names_and_uppercase_input(self):
        for name in prepare.WAD_NAMES:
            with self.subTest(name=name):
                manifest = prepare.prepare(self.output, wad=self.source(name.upper()))
                row = manifest['files'][0]
                self.assertEqual(row['file'], name)
                self.assertEqual(row['source'], str((self.root / name.upper()).resolve()))
                self.assertEqual(row['lumps'], 4)
                self.assertEqual((self.output / name).read_bytes(), self.data)
                self.assertEqual(json.loads((self.output / prepare.MANIFEST_NAME).read_text()), manifest)

    def test_reject_unsupported_name_and_pwad_without_output(self):
        for name, data in (('custom.wad', self.data), ('doom.wad', b'PWAD' + self.data[4:])):
            with self.subTest(name=name):
                with self.assertRaises(ValueError):
                    prepare.prepare(self.output, wad=self.source(name, data))
                self.assertFalse(self.output.exists())

    def test_package_output_is_rejected(self):
        for path in (prepare.APP_ROOT, prepare.APP_ROOT / 'assets', prepare.APP_ROOT / 'wad/files'):
            with self.subTest(path=path):
                with self.assertRaisesRegex(ValueError, 'outside the engine package'):
                    prepare.prepare(path, wad=self.source())

    def test_default_keeps_imported_wad_and_source_without_network(self):
        original = prepare.prepare(self.output, wad=self.source())
        with mock.patch.object(prepare.urllib.request, 'urlopen') as network:
            result = prepare.prepare(self.output, upstream=self.root / 'does-not-exist')
        network.assert_not_called()
        self.assertEqual(result, original)
        self.assertFalse((self.output / 'doom1.wad').exists())

    def test_existing_wad_without_manifest_is_kept(self):
        self.output.mkdir(parents=True)
        (self.output / 'doom.wad').write_bytes(self.data)
        with mock.patch.object(prepare.urllib.request, 'urlopen') as network:
            result = prepare.prepare(self.output)
        network.assert_not_called()
        self.assertEqual(result['files'][0]['file'], 'doom.wad')

    def test_explicit_import_selects_new_wad_without_deleting_other_imports(self):
        prepare.prepare(self.output, wad=self.source('doom2.wad'))
        selected = prepare.prepare(self.output, wad=self.source('doom.wad', make_wad(b'MAP01')))
        self.assertTrue((self.output / 'doom2.wad').exists())
        self.assertEqual(selected['files'][0]['file'], 'doom.wad')
        self.assertEqual(prepare.prepare(self.output), selected)

    def test_explicit_import_replaces_same_name(self):
        source = self.source()
        prepare.prepare(self.output, wad=source)
        updated = make_wad(b'MAP01')
        source.write_bytes(updated)
        result = prepare.prepare(self.output, wad=source)
        self.assertEqual((self.output / source.name).read_bytes(), updated)
        self.assertEqual(result['files'][0]['sha256'], hashlib.sha256(updated).hexdigest())

    def test_invalid_existing_wad_requires_explicit_import(self):
        self.output.mkdir(parents=True)
        (self.output / 'doom1.wad').write_bytes(b'corrupt')
        with mock.patch.object(prepare.urllib.request, 'urlopen') as network:
            with self.assertRaisesRegex(ValueError, 'Existing WAD is invalid'):
                prepare.prepare(self.output)
        network.assert_not_called()
        prepare.prepare(self.output, wad=self.source('doom1.wad'))

    def test_damaged_manifest_can_be_rebuilt(self):
        self.output.mkdir(parents=True)
        (self.output / 'doom.wad').write_bytes(self.data)
        for damaged in ('{', '[]', '{"files": null}', '{"files": [3]}'):
            with self.subTest(damaged=damaged):
                (self.output / prepare.MANIFEST_NAME).write_text(damaged)
                self.assertEqual(prepare.prepare(self.output)['files'][0]['file'], 'doom.wad')

    def test_pinned_download_url_and_bounded_read(self):
        response = mock.MagicMock()
        response.__enter__.return_value.read.return_value = self.data
        with mock.patch.object(prepare, 'SHAREWARE_SHA256', hashlib.sha256(self.data).hexdigest()), \
                mock.patch.object(prepare, 'SHAREWARE_BYTES', len(self.data)), \
                mock.patch.object(prepare.urllib.request, 'urlopen', return_value=response) as network:
            manifest = prepare.prepare(self.output)
        network.assert_called_once_with(
            'https://raw.githubusercontent.com/Daivuk/PureDOOM/'
            '355cfbd16fac119718879239336ee2ea408886bd/doom1.wad', timeout=60)
        response.__enter__.return_value.read.assert_called_once_with(prepare.MAX_WAD_BYTES + 1)
        self.assertEqual(manifest['files'][0]['source'], prepare.SHAREWARE_URL)

    def test_local_shareware_digest_mismatch_never_writes(self):
        with self.assertRaisesRegex(ValueError, 'SHA-256/size mismatch'):
            prepare.prepare(self.output, upstream=self.source('doom1.wad'))
        self.assertFalse(self.output.exists())

    def test_download_digest_mismatch_never_writes(self):
        response = mock.MagicMock()
        response.__enter__.return_value.read.return_value = self.data
        with mock.patch.object(prepare.urllib.request, 'urlopen', return_value=response):
            with self.assertRaisesRegex(ValueError, 'SHA-256/size mismatch'):
                prepare.prepare(self.output)
        self.assertFalse(self.output.exists())

    def test_failed_manifest_write_recovers_complete_wad_on_next_run(self):
        original = prepare.atomic_write

        def fail_manifest(path, data):
            if path.name == prepare.MANIFEST_NAME:
                raise OSError('simulated metadata interruption')
            original(path, data)

        with mock.patch.object(prepare, 'atomic_write', side_effect=fail_manifest):
            with self.assertRaises(OSError):
                prepare.prepare(self.output, wad=self.source())
        self.assertEqual((self.output / 'doom2.wad').read_bytes(), self.data)
        self.assertEqual(prepare.prepare(self.output)['files'][0]['file'], 'doom2.wad')
        self.assertTrue((self.output / prepare.MANIFEST_NAME).is_file())

    def test_import_replaces_oversized_existing_file(self):
        self.output.mkdir(parents=True)
        destination = self.output / 'doom2.wad'
        with destination.open('wb') as stream:
            stream.truncate(prepare.MAX_WAD_BYTES + 1)
        prepare.prepare(self.output, wad=self.source())
        self.assertEqual(destination.read_bytes(), self.data)

    def test_atomic_replace_failure_preserves_old_file_and_removes_temporary(self):
        self.output.mkdir(parents=True)
        destination = self.output / 'doom.wad'
        destination.write_bytes(b'old')
        with mock.patch.object(prepare.os, 'replace', side_effect=OSError('simulated interruption')):
            with self.assertRaises(OSError):
                prepare.atomic_write(destination, self.data)
        self.assertEqual(destination.read_bytes(), b'old')
        self.assertEqual(list(self.output.iterdir()), [destination])

    def test_linked_outputs_are_rejected_and_target_is_unchanged(self):
        self.output.mkdir(parents=True)
        target = self.source('private-looking-target', b'unchanged')
        for name in ('doom2.wad', prepare.MANIFEST_NAME):
            with self.subTest(name=name):
                link = self.output / name
                link.symlink_to(target)
                with self.assertRaisesRegex(ValueError, 'linked'):
                    prepare.prepare(self.output, wad=self.source())
                self.assertEqual(target.read_bytes(), b'unchanged')
                link.unlink()


class DeploymentTests(ArtifactTest):
    publisher = 'a' * 64
    remote_root = 'sd/PXA/media/publisher/pxa-doom'

    def setUp(self):
        super().setUp()
        self.manifest = prepare.prepare(self.output, wad=self.source())
        self.client = mock.MagicMock()
        self.client.device_info = 'serial=TEST-DEVICE;fs_sha256=1'
        self.client.__enter__.return_value = self.client
        self.packages = [frame('PKG', self.publisher + ':pxa-doom\tDOOM\t1\tinstalled=1;active=0')]
        self.paths = [frame('PATH', 'private\tsd/PXA/data/doom'),
                      frame('PATH', 'media\t' + self.remote_root)]

        def request(command):
            if command == 'PACKAGES':
                return self.packages
            if command == 'PACKAGE paths ' + self.publisher + ':pxa-doom':
                return self.paths
            self.fail('Unsupported command: ' + command)

        self.client.request.side_effect = request
        self.fs = mock.Mock()
        self.fs.list.return_value = [('D', 0, name) for name in ('sd', 'PXA', 'media', 'publisher', 'pxa-doom')]
        self.pxadb = SimpleNamespace(
            open_client=mock.Mock(return_value=self.client),
            info_properties=lambda text: dict(field.split('=', 1) for field in text.split(';') if '=' in field),
            NormalFsClient=mock.Mock(return_value=self.fs))

    def deploy(self, **kwargs):
        options = dict(port='FAKE-PORT', serial='TEST-DEVICE', output=self.output,
                       manifest=self.manifest, publisher=self.publisher)
        options.update(kwargs)
        return prepare.deploy_media(self.pxadb, **options)

    def test_uploads_only_selected_wad_to_host_media_root(self):
        remote = self.deploy()
        self.assertEqual(remote, self.remote_root + '/doom2.wad')
        self.pxadb.open_client.assert_called_once_with('FAKE-PORT', 15)
        self.fs.put.assert_called_once_with(self.output / 'doom2.wad', remote)
        self.fs.remove.assert_not_called()
        self.assertEqual(self.client.request.call_count, 2)

    def test_wrong_or_missing_device_serial_causes_no_queries_or_writes(self):
        for info in ('serial=OTHER', 'board=test'):
            with self.subTest(info=info):
                self.client.device_info = info
                with self.assertRaisesRegex(ValueError, 'serial'):
                    self.deploy()
        self.client.request.assert_not_called()
        self.pxadb.NormalFsClient.assert_not_called()

    def test_wrong_publisher_missing_install_and_active_app_refused(self):
        cases = [[], [frame('PKG', 'b' * 64 + ':pxa-doom\tDOOM\t1\tinstalled=1')],
                 [frame('PKG', self.publisher + ':pxa-doom\tDOOM\t1\tinstalled=0')],
                 self.packages + [frame('PKG', 'b' * 64 + ':pxa-other\tOther\t1\tactive=1')]]
        for packages in cases:
            with self.subTest(packages=packages):
                self.packages = packages
                with self.assertRaises(ValueError):
                    self.deploy()
        self.pxadb.NormalFsClient.assert_not_called()

    def test_unavailable_or_unsafe_media_paths_refused(self):
        cases = [[], [frame('OK', 'media=unavailable')]]
        for path in ('/sd/PXA/media', 'sd/../media', 'sd//media', 'sd/./media',
                     'sd\\media', 'sd/media\n'):
            cases.append([frame('PATH', 'media\t' + path)])
        cases.append(self.paths + [frame('PATH', 'media\t' + self.remote_root)])
        for paths in cases:
            with self.subTest(paths=paths):
                self.paths = paths
                with self.assertRaises(ValueError):
                    self.deploy()
        self.pxadb.NormalFsClient.assert_not_called()

    def test_changed_local_wad_refused_before_connecting(self):
        (self.output / 'doom2.wad').write_bytes(make_wad(b'MAP01'))
        with self.assertRaisesRegex(ValueError, 'changed'):
            self.deploy()
        self.pxadb.open_client.assert_not_called()

    def test_changed_wad_during_media_resolution_refused_before_upload(self):
        self.fs.list.side_effect = lambda _path: self.modify_local_and_list()
        with self.assertRaisesRegex(ValueError, 'changed'):
            self.deploy()
        self.fs.put.assert_not_called()

    def modify_local_and_list(self):
        (self.output / 'doom2.wad').write_bytes(make_wad(b'MAP01'))
        return [('D', 0, name) for name in ('sd', 'PXA', 'media', 'publisher', 'pxa-doom')]

    def test_invalid_publisher_or_missing_serial_rejected_before_connection(self):
        for options in ({'publisher': '../bad'}, {'publisher': 'A' * 64}, {'serial': ''}, {'port': ''}):
            with self.subTest(options=options):
                with self.assertRaises(ValueError):
                    self.deploy(**options)
        self.pxadb.open_client.assert_not_called()

    def test_publisher_default_reads_only_public_key(self):
        public = self.root / '.dev-signing/publisher-public.der'
        public.parent.mkdir()
        public.write_bytes(b'public DER test fixture')
        with mock.patch.object(prepare, 'APPS_ROOT', self.root):
            self.assertEqual(prepare.publisher_id(), hashlib.sha256(public.read_bytes()).hexdigest())

    def test_media_parent_creation_uses_filesystem_api(self):
        self.fs.list.side_effect = lambda path: [('D', 0, 'sd')] if path == '' else []
        self.deploy()
        self.assertEqual(self.fs.mkdir.call_args_list,
                         [mock.call('sd/PXA'), mock.call('sd/PXA/media'),
                          mock.call('sd/PXA/media/publisher'), mock.call(self.remote_root)])

    def test_unavailable_volume_never_creates_fallback(self):
        self.fs.list.return_value = []
        with self.assertRaisesRegex(ValueError, 'volume is unavailable'):
            self.deploy()
        self.fs.mkdir.assert_not_called()
        self.fs.put.assert_not_called()


class CommandLineTests(ArtifactTest):
    def test_help_has_all_requested_flags(self):
        output = io.StringIO()
        with contextlib.redirect_stdout(output):
            with self.assertRaises(SystemExit) as result:
                prepare.main(['--help'])
        self.assertEqual(result.exception.code, 0)
        for option in ('--wad', '--upstream', '--output', '--port', '--baud', '--serial', '--publisher-id'):
            self.assertIn(option, output.getvalue())

    def test_serial_required_and_bad_baud_before_preparation(self):
        for arguments in (['--port', 'FAKE'], ['--baud', '0'], ['--baud', 'invalid']):
            with self.subTest(arguments=arguments), contextlib.redirect_stderr(io.StringIO()), \
                    mock.patch.object(prepare, 'prepare') as function:
                with self.assertRaises(SystemExit) as result:
                    prepare.main(arguments)
                self.assertEqual(result.exception.code, 2)
                function.assert_not_called()

    def test_offline_import_command(self):
        with contextlib.redirect_stdout(io.StringIO()) as output:
            self.assertEqual(prepare.main(['--wad', str(self.source()), '--output', str(self.output)]), 0)
        self.assertIn('4 lumps', output.getvalue())
        self.assertTrue((self.output / prepare.MANIFEST_NAME).is_file())


if __name__ == '__main__':
    unittest.main()
