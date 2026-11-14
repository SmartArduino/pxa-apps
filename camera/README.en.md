# Camera

Camera saves photos and videos in its private `album/` directory. Existing files remain available after a normal app update.

Open a photo or video, choose **删除 (Delete)**, check the complete filename, then choose **删除 (Delete)** or **取消 (Cancel)**. Only the selected file is removed. The gallery returns to its first page and refreshes its count after deletion. Cancel preserves the file.

Viewing and thumbnails support JPEG photos or individual video frames up to 512 KiB encoded. Larger original photos remain saved; the gallery shows a clear viewing-limit message and still allows selecting and deleting them.

Pause video playback and wait for loading to finish before deleting. Deletion is unavailable while capturing, recording, playing, or loading. The app releases the selected image and playback file before removing it.

Use the supported PXA SDK to build a signed package. Camera requires the workspace peripherals extension; available capture profiles depend on the device.
