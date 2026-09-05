# Windows 产品版本组合

`product-lock.json` 是一个 Windows 源码版本对应的完整一方依赖组合：Server、页面、安装器、辅助码的完整提交，Server 实际引用的 Engine/GUI gitlink，以及词库 Release 的每个产物 SHA256。

发布任务不解析 `main` 或 `latest`。Server 构建和安装器收集资源使用相同提交；发布辅助脚本也来自 TSF 的发布提交。递归 submodule 沿固定 gitlink 检出，不执行 `git submodule update --remote`。

## 更新依赖

显式刷新后评审并提交清单，产品 CI 通过后才能发布：

```sh
python scripts/product_lock.py refresh --dictionary-tag dict-2026.09.05
python scripts/product_lock.py validate
```

`refresh` 是唯一会解析浮动源码引用的命令。默认解析各组件 main；可用 `--ref server=<SHA或分支>`、`--ref ui=...`、`--ref installer=...`、`--ref helpcode=...` 测试跨仓变更。Engine 和 GUI 必须取自选定 Server 的 gitlink，不能独立覆盖成未经集成的版本。

词库 tag 本身可能被上游修改，因此构建验证的是本清单中已提交的摘要，连同模型授权文件和上游校验文件一起验证。只更新上游 SHA256SUMS 无法让被替换的数据通过构建。

## 重建与追溯

手动 Release 只接受已有 draft tag，读取该提交内的清单。变更词库需要新的源码提交和产品版本，不允许临时覆盖词库 tag。没有清单的历史版本不能用新流水线宣称可复现重建。

发布前 `product_lock.py verify-published` 要求清单里每个提交都能从各自仓库的默认分支到达。这一条只在发布路径上执行，不进 `product-ci.yml`：跨仓改动同时落地时，PR 里锁住分支提交是正常且必要的，在 PR CI 上强制会把它本该保护的那次落地直接锁死。到了发布这一刻判断相反——没进默认分支的输入就是没人合过的输入，锁文件为它背书等于宣称了一次并不存在的评审。

安装包携带 `%LOCALAPPDATA%/metasequoiaime/product-manifest.json`，Release 同时附加同名文件。其中包含 TSF 提交、全部锁定输入和清单文件摘要。签名时间、工具链与 runner 更新仍可能改变二进制字节；此机制保证产品源码与数据组合确定，不承诺安装包逐字节一致。

本地检查：

```sh
python -m unittest discover -s tests -p 'test_product_lock.py'
python scripts/product_lock.py verify-checkout server ../MSIME-Server
python scripts/product_lock.py fetch-dictionaries --staging-root /tmp/msime-product-data
```
