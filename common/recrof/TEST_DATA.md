# 本地验收资源

这些数据只用于本地验收，导入目录与元数据由 .gitignore 排除。Quake 的个人数据在导入前使用仓库中的格式 3 转换器；Duke3D 支持原始 GRP 和已转换 GRP，Quake II、C&C、RA 使用归档中的原始游戏数据。完整零售版未作验收。

| 数据 | 原始归档 | SHA-256 |
| --- | --- | --- |
| Quake 1.06 shareware | [quake106.zip](https://ftp.gwdg.de/pub/misc/ftp.idsoftware.com/idstuff/quake/quake106.zip) | `ec6c9d34b1ae0252ac0066045b6611a7919c2a0d78a3a66d9387a8f597553239` |
| Quake II 3.14 demo | [q2-demo.exe](https://www.boxedwine.org/wp-content/uploads/2018/02/q2-314-demo-x86.exe) | `7ace5a43983f10d6bdc9d9b6e17a1032ba6223118d389bd170df89b945a04a1e` |
| Duke3D 1.3D shareware | [3dduke13.zip](https://ftp.gamefilearchive.com/ftp.3drealms.com/share/3dduke13.zip) | `c67efd179022bc6d9bde54f404c707cbcbdc15423c20be72e277bc2bdddf3d0e` |
| C&C DOS demo | [cc1demo1.zip](https://www.classicdosgames.com/files/games/virgin/cc1demo1.zip) | `29d33849cb450a87262426cc59b25baa5e033b94c2e3495ec94f813f3bc09f87` |
| Red Alert 95 demo | [ra95demo.zip](https://www.dosgamesarchive.com/file.php?id=946) | `8c48bdb53523ea4d353a7914d9234ce4e3e6f5832eeb47b3b6b005853e8748d2` |

C&C 使用 `cc1demo1.zip` 中的 DOS DEMO/DEMOL/SOUNDS/SPEECH MIX；RA 使用 `ra95demo/INSTALL` 中的 REDALERT.MIX 和 MAIN.MIX。转换后或导入后的逐文件校验和保存在`.game-data/<游戏>/imported-game-data.json`，实际资源位于同级 `files/`（不会被打入应用包）。
