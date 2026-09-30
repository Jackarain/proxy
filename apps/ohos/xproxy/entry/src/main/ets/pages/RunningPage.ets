import { common } from '@kit.AbilityKit';
import { router, promptAction } from '@kit.ArkUI';
import { http } from '@kit.NetworkKit';
import { formatBytes, formatSpeed } from '../common/Util';
import { VpnConfig } from '../model/VpnConfig';
import { AppSession } from '../service/AppSession';
import { StorageService } from '../service/StorageService';

/** status 通知里的全局统计. */
interface GlobalStats {
  rx_bytes?: number;
  tx_bytes?: number;
  conn_active?: number;
  conn_total?: number;
}

/** status 通知里的速率. */
interface Rates {
  rx_rate_bps?: number;
  tx_rate_bps?: number;
}

/** status 通知体. */
interface StatusSnapshot {
  global?: GlobalStats;
  rates?: Rates;
}

@Entry
@Component
struct RunningPage {
  @State connected: boolean = false;
  @State statusText: string = '等待控制通道连接';
  @State rxRate: string = '-';
  @State txRate: string = '-';
  @State rxTotal: string = '-';
  @State txTotal: string = '-';
  @State connText: string = '-';
  @State logs: string[] = [];
  @State latency: string = '';
  @State busy: boolean = false;
  @State configName: string = 'proxy';
  @State testUrl: string = 'https://www.google.com';
  private timer: number = -1;
  private session = AppSession.instance;

  aboutToAppear(): void {
    const context = this.getUIContext().getHostContext() as common.UIAbilityContext;
    const service = new StorageService(context);
    const configId: string = this.session.runningConfigId;
    const config: VpnConfig | undefined = service.loadConfigs()
      .find((item: VpnConfig) => item.id === configId);
    if (config !== undefined) {
      this.configName = config.name;
      this.testUrl = config.testUrl;
    }
    this.session.subscribe(() => {
      this.sync();
    });
    this.sync();
    this.timer = setInterval(() => {
      this.sync();
    }, 1000);
  }

  aboutToDisappear(): void {
    if (this.timer !== -1) {
      clearInterval(this.timer);
      this.timer = -1;
    }
  }

  private sync(): void {
    const session = this.session;
    this.connected = session.connected;
    this.logs = session.logs.slice(session.logs.length > 200 ? session.logs.length - 200 : 0);
    if (!session.isRunning()) {
      this.statusText = 'VPN 已停止';
      return;
    }
    if (session.statusJson.length === 0) {
      this.statusText = this.connected ? '已连接, 等待状态上报' : '等待控制通道连接';
      return;
    }
    this.statusText = this.connected ? '运行中' : '控制通道断开';
    try {
      const snapshot: StatusSnapshot = JSON.parse(session.statusJson) as StatusSnapshot;
      const global: GlobalStats = snapshot.global ?? {};
      const rates: Rates = snapshot.rates ?? {};
      this.rxRate = formatSpeed(rates.rx_rate_bps ?? 0);
      this.txRate = formatSpeed(rates.tx_rate_bps ?? 0);
      this.rxTotal = formatBytes(global.rx_bytes ?? 0);
      this.txTotal = formatBytes(global.tx_bytes ?? 0);
      this.connText = `${global.conn_active ?? 0} / ${global.conn_total ?? 0}`;
    } catch (err) {
      this.statusText = '状态解析失败';
    }
  }

  private async testConnection(): Promise<void> {
    if (this.busy) {
      return;
    }
    this.busy = true;
    this.latency = '测试中...';
    const client = http.createHttp();
    const started: number = Date.now();
    try {
      const resp = await client.request(this.testUrl, {
        method: http.RequestMethod.GET,
        connectTimeout: 10000,
        readTimeout: 15000
      });
      const cost: number = Date.now() - started;
      this.latency = `延迟: ${cost} ms (HTTP ${resp.responseCode})`;
    } catch (err) {
      this.latency = `测试失败: ${(err as Error).message}`;
    } finally {
      client.destroy();
      this.busy = false;
    }
  }

  private async stop(): Promise<void> {
    const context = this.getUIContext().getHostContext() as common.UIAbilityContext;
    this.busy = true;
    try {
      await this.session.stop(context);
      promptAction.showToast({ message: '已停止' });
      router.back();
    } catch (err) {
      promptAction.showToast({ message: `停止失败: ${(err as Error).message}` });
    } finally {
      this.busy = false;
    }
  }

  @Builder
  statCard(title: string, value: string) {
    Column() {
      Text(title)
        .fontSize(12)
        .fontColor('#888888')
      Text(value.length > 0 ? value : '-')
        .fontSize(16)
        .fontWeight(FontWeight.Medium)
        .margin({ top: 6 })
    }
    .layoutWeight(1)
    .padding(12)
    .backgroundColor(Color.White)
  }

  build() {
    Column() {
      Row() {
        Text('返回')
          .fontSize(16)
          .onClick(() => {
            router.back();
          })
        Text(this.configName)
          .fontSize(20)
          .fontWeight(FontWeight.Bold)
          .layoutWeight(1)
          .textAlign(TextAlign.Center)
        Text('停止')
          .fontSize(16)
          .fontColor(Color.Red)
          .onClick(() => {
            this.stop();
          })
      }
      .width('100%')
      .padding(12)

      Row() {
        Text(this.statusText)
          .fontSize(14)
          .fontColor(Color.White)
          .layoutWeight(1)
        if (this.latency.length > 0) {
          Text(this.latency)
            .fontSize(12)
            .fontColor(Color.White)
        }
      }
      .width('100%')
      .padding(12)
      .backgroundColor(this.connected ? '#2E7D32' : '#9E9E9E')

      Row({ space: 8 }) {
        this.statCard('上传速率', this.rxRate)
        this.statCard('下载速率', this.txRate)
      }
      .width('100%')
      .padding({ left: 12, right: 12, top: 8 })
      .backgroundColor('#F1F3F5')

      Row({ space: 8 }) {
        this.statCard('上行流量', this.rxTotal)
        this.statCard('下行流量', this.txTotal)
      }
      .width('100%')
      .padding({ left: 12, right: 12, top: 8 })
      .backgroundColor('#F1F3F5')

      Row({ space: 8 }) {
        this.statCard('连接数 (活跃/累计)', this.connText)
        this.statCard('隧道地址', 'vpn-tun')
      }
      .width('100%')
      .padding({ left: 12, right: 12, top: 8 })
      .backgroundColor('#F1F3F5')

      Row() {
        Button('测试连接')
          .layoutWeight(1)
          .enabled(!this.busy)
          .onClick(() => {
            this.testConnection();
          })
      }
      .width('100%')
      .padding(12)

      Text('日志')
        .fontSize(13)
        .fontColor('#666666')
        .width('100%')
        .padding({ left: 12, bottom: 4 })

      List() {
        ForEach(this.logs, (line: string) => {
          ListItem() {
            Text(line)
              .fontSize(12)
              .fontFamily('monospace')
              .width('100%')
          }
        }, (line: string, index: number) => `${index}-${line}`)
      }
      .layoutWeight(1)
      .width('100%')
      .padding({ left: 12, right: 12 })
      .backgroundColor('#111111')
    }
    .width('100%')
    .height('100%')
    .backgroundColor('#F1F3F5')
  }
}
