# Out-of-tree 驱动

存放本项目自研的 Zephyr 驱动（如专用 AFE 电池监测芯片驱动）。

## 接入步骤（待实现时）

1. 在本目录下按 Zephyr 驱动规范创建驱动（含 `CMakeLists.txt`、`Kconfig`、`zephyr/dts/bindings/` 绑定）。
2. 在根目录 `zephyr/module.yml` 的 `build.settings` 中启用：
   ```yaml
   build:
     cmake: drivers
     kconfig: drivers/Kconfig
   ```
3. 在 board overlay 中实例化对应 devicetree 节点。
4. AFE 模块（`app/src/bms/afe/afe.c`）通过 `DEVICE_DT_GET` 获取驱动并替换桩采样逻辑。

当前为占位，AFE 采样使用 native_sim 桩数据。
