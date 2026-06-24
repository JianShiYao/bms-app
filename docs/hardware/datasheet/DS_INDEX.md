# 数据手册索引（datasheets）

> 依据 [BOM_BPCB-PCB-MB-PD-S16100B_V0.4.md](../BOM_BPCB-PCB-MB-PD-S16100B_V0.4.md) 收集主要元器件数据手册，存于本目录（`doc/HW/datasheet/`）。
> 通用阻容、LED 等未单独收录。AFE 主芯片见本目录 `DS_SH36735XX CV0.2B.pdf`。

## ⚠️ 核验声明（安全攸关）

- 手册由联网检索下载，已逐份验证为**真实 PDF** 且**正文含对应型号**，但**来源厂商可能与实际上料器件不一致**（很多为多型号系列手册或替代厂商版本）。**采购/参数核验前必须对照实际器件丝印与原厂手册确认**。
- 标注 **「UNCONFIRMED」** 的为原理图未读到确切型号、按最可能型号下载的**候选**手册，使用前必须先确认实际型号。
- 部分国产/冷门器件无可靠来源，已**跳过不臆造**，见文末「未收录」清单。

## 已收录手册

| 文件 | 位号 | 器件 | 来源 | 验证 |
|---|---|---|---|---|
| DS_W25Q32JV.pdf | U600 | Winbond 32Mbit SPI NOR Flash（BOM: W25Q32JVSSIQ） | winbond.com（Rev G） | 含型号 ✓ |
| DS_LMV321.pdf | U104 | TI 单运放（电流检测放大） | ti.com/lit（SLOS263Y） | 含型号 ✓ |
| DS_ACM2520-301-2P.pdf | U301/U302 | TDK 共模电感（系列手册含 301-2P） | TDK 系列手册（LCSC CDN 镜像） | 含型号 ✓ |
| DS_SI2302.pdf | Q109 等 | Vishay Si2302 N-MOS（BOM: SI2302S） | vishay.com | 含型号 ✓ |
| DS_BSS123.pdf | Q103/Q417 | Diodes BSS123 N-MOS 100V | diodes.com | 含型号 ✓ |
| DS_CRSS042N10N.pdf | Q902/Q903 | CR Micro N-MOS 100V（充电功率级） | CR Micro 手册（elecfans 镜像，内容对应 LCSC C410926） | 含型号 ✓ |
| DS_HSS0127.pdf | Q101/Q900 | HUASHUO 华硕微 P-MOS 100V（负载开关） | LCSC CDN（C700960） | 含型号 ✓ |
| DS_MMBT5551.pdf | Q100/Q106/均衡阵列 | Diodes MMBT5551 NPN 高压 | diodes.com | 含型号 ✓ |
| DS_MMBT5401.pdf | Q901/Q4xx | Diodes MMBT5401 PNP 150V | diodes.com | 含型号 ✓ |
| DS_1N4148WS.pdf | D203-205/D3xx/D4xx | Taiwan Semi 开关二极管 SOD-323 | services.taiwansemi.com | 含型号 ✓ |
| DS_B5817WS.pdf | D101/D200/D201 | Rectron B5817WS 肖特基 SOD-323 | rectron.com | 含型号 ✓ |
| DS_SS310.pdf | D509 | Taiwan Semi 3A/100V 肖特基（SS32 系列含 SS310） | services.taiwansemi.com | 含系列 ✓ |
| DS_DSS110.pdf | D900 | High Diode DSS110 肖特基 1A | LCSC CDN（C466480） | 含型号 ✓ |
| DS_MMSZ5246B.pdf | D104/D407/D902/D905/D500/D508 | MMSZ5246B 稳压 ~16V | 内容已验证（含型号） | 含型号 ✓ |
| DS_BZG03C15.pdf | D405/D406 | BZG03C15 稳压 | 内容已验证（含型号） | 含型号 ✓ |
| DS_SMA4758A.pdf | D904 | SMA4758A 稳压 ~56V | 内容已验证（含型号） | 含型号 ✓ |
| DS_SMAJ6.5CA.pdf | D300-D310 | SMAJ6.5CA TVS（RS485 防护） | 内容已验证（含型号） | 含型号 ✓ |
| DS_Littelfuse_451.pdf | F501 | Littelfuse 451 系列保险丝（BOM: 0451010.MRL，~10A） | littelfuse 451 系列 | 含系列 ✓ |
| DS_BSMD1812.pdf | F300-F303 | BSMD1812 自恢复 PTC（1812） | 内容已验证（含型号） | 含型号 ✓ |
| DS_SWPA4030S101MT.pdf | L100/L900 | Sunlord 功率电感 100µH | Sunlord SWPA4030S 系列（alfatec 镜像） | 含型号 ✓ |
| DS_GZ1608D601TF.pdf | B300-B307 | Sunlord 叠层磁珠 0603 600Ω | Sunlord GZ 系列（huaqiu 镜像） | 含型号 ✓ |
| DS_MF52_NTC.pdf | NTC700-702/NTC900 | MF52 NTC 热敏电阻系列（10k/100k B3950） | Cantherm MF52 系列 | 含系列 ✓ |
| DS_TL494.pdf | 充电 PWM（U9xx） | TI TL494 PWM 控制器 — **UNCONFIRMED 候选** | ti.com/lit（SLVS074I） | **型号未经原理图确认** |

## 未收录（无可靠来源或型号未知，需人工确认）

| 部件 | 位号 | 原因 |
|---|---|---|
| SL3H7 光耦 | U309/U500/U501/U900/U902/U904 | 无原厂/经销商可靠手册；疑为 Everlight EL3H7 的别名或国产变体，需确认确切型号厂商 |
| CPU10N10 N-MOS | Q442 等（主回路功率管） | 仅聚合站，无原厂手册；可能为厂家内部/打标码，需确认订货型号 |
| HSS3401A MOS | Q441 | 无 HSS3401A 手册（搜索均指向不相关的 AO3401A），需确认 |
| DSS310 肖特基 | D100/D103 | 无原厂手册（仅聚合站）；注意已收录的是 DSS110，非 DSS310 |
| SMD75A TVS | D409-D423 阵列 | 无可靠来源，待确认 |
| CJSMF85A TVS | D401/D417 | 无可靠来源（疑江苏长晶 CJ），待确认 |
| **U200 主控 MCU** | U200 | 原理图符号内未印型号，需查 EDA 工程 |
| **U300/U305 通信 IC** | U300/U305 | 原理图被裁切未读到型号，需查工程 |
| **充电 PWM 控制器确切型号** | U9xx | 仅按引脚推断为 TL494/KA7500 类，已下载 TL494 作候选，**待确认** |

## 备注

- 个别经销商（onsemi、Nexperia、Mouser）对脚本下载返回 403/Access-Denied，故部分常规器件改用同等可靠厂商版本（如 Diodes Inc、Taiwan Semiconductor、Rectron）。这些**替代厂商手册的参数需与实际器件比对后方可采用**。
- `DS_TL494.pdf` 为候选，切勿在未确认型号前据其编写固件或保护逻辑。
