#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
import importlib.util
from pathlib import Path
import threading
import unittest
import urllib.error
import urllib.request
import json

spec=importlib.util.spec_from_file_location('wifi_bridge',Path(__file__).parents[1]/'tools/wifi_bridge.py')
bridge=importlib.util.module_from_spec(spec);spec.loader.exec_module(bridge)
class BridgeTest(unittest.TestCase):
    def test_escaped_ssids_and_filters(self):
        data=r'00\:11\:22\:33\:44\:55:Office\: AP\\west'+'\n'+r'00\:11\:22\:33\:44\:55:duplicate'+'\n'+r'02\:11\:22\:33\:44\:56:private'+'\n'+r'00\:11\:22\:33\:44\:57:hidden_nomap'+'\n'
        result=bridge.access_points(data)
        self.assertEqual(result,[{'macAddress':'00:11:22:33:44:55','ssid':'Office: AP\\west'}])
    def test_age_is_not_refreshed_by_reads(self):
        now=[100.0];latest=bridge.Latest(lambda:now[0]);value={'lat':22.5,'lng':114.0,'accuracy':10,'datum':'wgs84'}
        latest.publish(value,98);a=latest.get();now[0]=104;b=latest.get()
        self.assertEqual(a['sample_id'],b['sample_id']);self.assertEqual(b['age_ms'],6000)
        now[0]=129;self.assertIsNone(latest.get())
        latest.publish(value,129);self.assertNotEqual(a['sample_id'],latest.get()['sample_id'])
        latest.clear();self.assertIsNone(latest.get())
    def test_http_auth_missing_fix_and_cache(self):
        latest=bridge.Latest();server=bridge.ThreadingHTTPServer(('127.0.0.1',0),bridge.handler(latest,'token'))
        thread=threading.Thread(target=server.serve_forever,daemon=True);thread.start()
        base=f'http://127.0.0.1:{server.server_address[1]}'
        try:
            for path,code in [('/fix/wrong',404),('/fix/token',503)]:
                with self.assertRaises(urllib.error.HTTPError)as error:urllib.request.urlopen(base+path)
                self.assertEqual(error.exception.code,code);error.exception.close()
            latest.publish({'lat':22.5,'lng':114.0,'accuracy':10,'datum':'wgs84'},latest.clock())
            with urllib.request.urlopen(base+'/fix/token')as response:
                self.assertEqual(response.headers['Cache-Control'],'no-store');self.assertIn('sample_id',json.load(response))
        finally:server.shutdown();server.server_close();thread.join()
if __name__=='__main__':unittest.main()
