import { common } from '@kit.AbilityKit';
import { router, promptAction } from '@kit.ArkUI';
import { http } from '@kit.NetworkKit';
import { VpnConfig, splitForeignDns, ForeignDns } from '../model/VpnConfig';
import { joinList, splitList } from '../common/Util';
import { AppSession } from '../service/AppSession';
import { StorageService } from '../service/StorageService';

/** 路由参数. */
interface EditParams {
  id?: string;
}

@Entry
@Component
struct ConfigEditPage {
  @State name: string = '未命名';
  @State proxyPass: string = '';
  @State sni: string = '';
  @State tunMtu: string = '1500';
  @State globalProxy: boolean = false;
  @State proxyDomainsText: string = '';
  @State proxyCidrText: string = '';
  @State proxyDomainsUrl: string = '';
  @State proxyCidrUrl: string = '';
  @State dnsText: string = '223.6.6.6\n119.29.29.29';
  @State dnsForeignText: string = '';
  @State dnsCache: boolean = true;
  @State noIpv6: boolean = true;
  @State udpTimeout: string = '300';
  @State poolSize: string = '20';
  @State disableCheckCert: boolean = true;
  @State tunAddress: string = '';
  @State tunPrefix: string = '';
  @State testUrl: string = 'https://www.google.com';
  @State bypassCn: boolean = false;
  @State busy: boolean = false;

  private configId: string = '';
  private service: StorageService | undefined = undefined;
  private allConfigs: VpnConfig[] = [];

  aboutToAppear(): void {
    const context = this.getUIContext().getHostContext() as common.UIAbilityContext;
    this.service = new StorageService(context);
    this.allConfigs = this.service.loadConfigs();
    const params = router.getParams() as EditParams;
    const id: string = params !== undefined && params.id !== undefined ? params.id : '';
    if (id.length === 0) {
      const config = VpnConfig.create('新配置');
      this.configId = config.id;
      this.loadFrom(config);
      return;
    }
    const existing: VpnConfig | undefined = this.allConfigs.find((item: VpnConfig) => item.id === id);
    if (existing !== undefined) {
      this.configId = existing.id;
      this.loadFrom(existing);
    } else {
      const config = VpnConfig.create('新配置');
      this.configId = config.id;
      this.loadFrom(config);
    }
  }

  private loadFrom(config: VpnConfig): void {
    this.name = config.name;
    this.proxyPass = config.proxyPass;
    this.sni = config.sni;
    this.tunMtu = `${config.tunMtu}`;
    this.globalProxy = config.globalProxy;
    this.proxyDomainsText = joinList(config.proxyDomains);
    this.proxyCidrText = joinList(config.proxyCidr);
    this.proxyDomainsUrl = config.proxyDomainsUrl;
    this.proxyCidrUrl = config.proxyCidrUrl;
    this.dnsText = joinList(config.dns);
    this.dnsForeignText = joinList(config.joinForeignDns());
    this.dnsCache = config.dnsCache;
    this.noIpv6 = config.noIpv6;
    this.udpTimeout = `${config.udpTimeout}`;
    this.poolSize = `${config.proxyPassPoolSize}`;
    this.disableCheckCert = config.disableCheckCert;
    this.tunAddress = config.tunAddress;
    this.tunPrefix = config.tunPrefix > 0 ? `${config.tunPrefix}` : '';
    this.testUrl = config.testUrl;
    this.bypassCn = config.bypassCn;
  }

  private collect(): VpnConfig {
    const config = new VpnConfig();
    config.id = this.configId;
    config.name = this.name.trim().length > 0 ? this.name.trim() : '未命名';
    config.proxyPass = this.proxyPass;
    config.sni = this.sni;
    config.tunMtu = toInt(this.tunMtu, 1500);
    config.globalProxy = this.globalProxy;
    config.proxyDomains = splitList(this.proxyDomainsText);
    config.proxyCidr = splitList(this.proxyCidrText);
    config.proxyDomainsUrl = this.proxyDomainsUrl.trim();
    config.proxyCidrUrl = this.proxyCidrUrl.trim();
    config.dns = splitList(this.dnsText);
    if (config.dns.length === 0) {
      config.dns = ['223.6.6.6', '119.29.29.29'];
    }
    const foreign: ForeignDns = splitForeignDns(splitList(this.dnsForeignText));
    config.dnsForeign = foreign.ips;
    config.dnsForeignDoh = foreign.doh;
    config.dnsCache = this.dnsCache;
    config.noIpv6 = this.noIpv6;
    config.udpTimeout = toInt(this.udpTimeout, 300);
    config.proxyPassPoolSize = toInt(this.poolSize, 20);
    config.disableCheckCert = this.disableCheckCert;
    config.tunAddress = this.tunAddress.trim();
    config.tunPrefix = toInt(this.tunPrefix, 0);
    config.testUrl = this.testUrl.trim();
    config.bypassCn = this.bypassCn;
    return config;
  }

  private async persist(run: boolean): Promise<void> {
    const service = this.service;
    if (service === undefined) {
      return;
    }
    const config: VpnConfig = this.collect();
    const errors: string[] = config.validate();
    if (errors.length > 0) {
      promptAction.showToast({ message: errors[0] });
      return;
    }
    const index: number = this.allConfigs.findIndex((item: VpnConfig) => item.id === config.id);
    if (index >= 0) {
      this.allConfigs[index] = config;
    } else {
      this.allConfigs.push(config);
    }
    service.saveConfigs(this.allConfigs);
    if (run) {
      await this.startRun(config);
      return;
    }
    promptAction.showToast({ message: '已保存' });
    router.back();
  }

  private async startRun(config: VpnConfig): Promise<void> {
    const context = this.getUIContext().getHostContext() as common.UIAbilityContext;
    this.busy = true;
    try {
      if (AppSession.instance.isRunning()) {
        await AppSession.instance.applyConfig(context, config);
      } else {
        await AppSession.instance.start(context, config);
      }
      router.pushUrl({ url: 'pages/RunningPage' });
    } catch (err) {
      promptAction.showToast({ message: `启动失败: ${(err as Error).message}` });
    } finally {
      this.busy = false;
    }
  }

  /** 从 URL 拉取列表 (每行一项), 填入对应输入框. */
  private async fetchList(url: string, target: number): Promise<void> {
    if (url.trim().length === 0) {
      promptAction.showToast({ message: '请先填写拉取地址' });
      return;
    }
    const client = http.createHttp();
    try {
      const resp = await client.request(url.trim(), {
        method: http.RequestMethod.GET,
        expectDataType: http.HttpDataType.STRING,
        connectTimeout: 15000,
        readTimeout: 30000,
        maxLimit: 8 * 1024 * 1024
      });
      if (resp.responseCode !== 200) {
        promptAction.showToast({ message: `拉取失败: HTTP ${resp.responseCode}` });
        return;
      }
      const text: string = typeof resp.result === 'string' ? resp.result : '';
      const items: string[] = text.split(/[\r\n]+/)
        .map((line: string) => line.trim())
        .filter((line: string) => line.length > 0 && line.indexOf('/') >= 0);
      if (items.length === 0) {
        promptAction.showToast({ message: '拉取结果为空' });
        return;
      }
      if (target === 0) {
        this.proxyDomainsText = items.join('\n');
      } else {
        this.proxyCidrText = items.join('\n');
      }
      promptAction.showToast({ message: `已拉取 ${items.length} 条` });
    } catch (err) {
      promptAction.showToast({ message: `拉取失败: ${(err as Error).message}` });
    } finally {
      client.destroy();
    }
  }

  @Builder
  fieldLabel(text: string) {
    Text(text)
      .fontSize(13)
      .fontColor('#666666')
      .width('100%')
      .margin({ top: 12, bottom: 4 })
  }

  @Builder
  switchRow(title: string, checked: boolean, onChange: (value: boolean) => void) {
    Row() {
      Text(title)
        .fontSize(15)
        .layoutWeight(1)
      Toggle({ type: ToggleType.Switch, isOn: checked })
        .onChange((value: boolean) => {
          onChange(value);
        })
    }
    .width('100%')
    .padding({ top: 10, bottom: 10 })
  }

  build() {
    Column() {
      Row() {
        Text('返回')
          .fontSize(16)
          .onClick(() => {
            router.back();
          })
        Text('配置')
          .fontSize(20)
          .fontWeight(FontWeight.Bold)
          .layoutWeight(1)
          .textAlign(TextAlign.Center)
        Text('保存')
          .fontSize(16)
          .onClick(() => {
            this.persist(false);
          })
      }
      .width('100%')
      .padding(12)

      Scroll() {
        Column() {
          this.fieldLabel('配置名称')
          TextInput({ text: this.name })
            .onChange((value: string) => {
              this.name = value;
            })

          this.fieldLabel('上游代理地址 (http/https/socks4/socks5/socks5s)')
          TextInput({ text: this.proxyPass, placeholder: 'https://user:pass@host:443' })
            .type(InputType.Normal)
            .onChange((value: string) => {
              this.proxyPass = value;
            })

          this.fieldLabel('TLS SNI (留空使用主机名)')
          TextInput({ text: this.sni })
            .onChange((value: string) => {
              this.sni = value;
            })

          this.switchRow('全局代理', this.globalProxy, (value: boolean) => {
            this.globalProxy = value;
          })
          Text('开启后所有流量均走上游代理, 忽略下方分流规则')
            .fontSize(12)
            .fontColor('#999999')
            .width('100%')

          if (!this.globalProxy) {
            this.fieldLabel('代理域名 (后缀匹配, 每行一个)')
            TextArea({ text: this.proxyDomainsText, placeholder: 'google.com' })
              .height(120)
              .onChange((value: string) => {
                this.proxyDomainsText = value;
              })
            Row() {
              TextInput({ text: this.proxyDomainsUrl, placeholder: '从 URL 拉取域名列表' })
                .layoutWeight(1)
                .onChange((value: string) => {
                  this.proxyDomainsUrl = value;
                })
              Button('拉取')
                .margin({ left: 8 })
                .onClick(() => {
                  this.fetchList(this.proxyDomainsUrl, 0);
                })
            }
            .width('100%')
            .margin({ top: 8 })

            this.fieldLabel('代理 CIDR (每行一个)')
            TextArea({ text: this.proxyCidrText, placeholder: '1.1.1.0/24' })
              .height(100)
              .onChange((value: string) => {
                this.proxyCidrText = value;
              })
            Row() {
              TextInput({ text: this.proxyCidrUrl, placeholder: '从 URL 拉取 CIDR 列表' })
                .layoutWeight(1)
                .onChange((value: string) => {
                  this.proxyCidrUrl = value;
                })
              Button('拉取')
                .margin({ left: 8 })
                .onClick(() => {
                  this.fetchList(this.proxyCidrUrl, 1);
                })
            }
            .width('100%')
            .margin({ top: 8 })
          }

          this.fieldLabel('国内 DNS (仅 IP, 每行一个)')
          TextArea({ text: this.dnsText })
            .height(80)
            .onChange((value: string) => {
              this.dnsText = value;
            })

          this.fieldLabel('国外 DNS / DoH (IP 每行一个, DoH 以 http(s):// 开头)')
          TextArea({ text: this.dnsForeignText, placeholder: '8.8.8.8\nhttps://dns.google/dns-query' })
            .height(90)
            .onChange((value: string) => {
              this.dnsForeignText = value;
            })

          this.switchRow('DNS 结果缓存', this.dnsCache, (value: boolean) => {
            this.dnsCache = value;
          })
          this.switchRow('禁用 IPv6 解析', this.noIpv6, (value: boolean) => {
            this.noIpv6 = value;
          })

          this.fieldLabel('TUN MTU')
          TextInput({ text: this.tunMtu })
            .type(InputType.Number)
            .onChange((value: string) => {
              this.tunMtu = value;
            })

          this.fieldLabel('TUN 地址 (留空使用 10.0.0.2/24)')
          Row() {
            TextInput({ text: this.tunAddress, placeholder: '10.0.0.2' })
              .layoutWeight(2)
              .onChange((value: string) => {
                this.tunAddress = value;
              })
            TextInput({ text: this.tunPrefix, placeholder: '24' })
              .type(InputType.Number)
              .layoutWeight(1)
              .margin({ left: 8 })
              .onChange((value: string) => {
                this.tunPrefix = value;
              })
          }
          .width('100%')

          this.fieldLabel('UDP 超时 (秒)')
          TextInput({ text: this.udpTimeout })
            .type(InputType.Number)
            .onChange((value: string) => {
              this.udpTimeout = value;
            })

          this.fieldLabel('上游连接池大小 (0 禁用)')
          TextInput({ text: this.poolSize })
            .type(InputType.Number)
            .onChange((value: string) => {
              this.poolSize = value;
            })

          this.fieldLabel('测试连接地址')
          TextInput({ text: this.testUrl })
            .onChange((value: string) => {
              this.testUrl = value;
            })

          this.switchRow('关闭证书校验', this.disableCheckCert, (value: boolean) => {
            this.disableCheckCert = value;
          })
          this.switchRow('绕过中国大陆', this.bypassCn, (value: boolean) => {
            this.bypassCn = value;
          })
          Text('仅非中国 IP 段接入 VPN, 中国大陆流量直连 (路由上限 1024 条)')
            .fontSize(12)
            .fontColor('#999999')
            .width('100%')

          Row() {
            Button('保存')
              .layoutWeight(1)
              .onClick(() => {
                this.persist(false);
              })
            Button('保存并运行')
              .layoutWeight(1)
              .margin({ left: 12 })
              .enabled(!this.busy)
              .onClick(() => {
                this.persist(true);
              })
          }
          .width('100%')
          .margin({ top: 24, bottom: 32 })
        }
        .width('100%')
        .padding({ left: 16, right: 16 })
      }
      .layoutWeight(1)
      .scrollBar(BarState.Auto)
    }
    .width('100%')
    .height('100%')
    .backgroundColor('#F1F3F5')
  }
}

function toInt(text: string, fallback: number): number {
  const value: number = Number.parseInt(text.trim(), 10);
  return Number.isFinite(value) ? value : fallback;
}
