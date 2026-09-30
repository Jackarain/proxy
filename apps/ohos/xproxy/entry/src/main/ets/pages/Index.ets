import { common, Want } from '@kit.AbilityKit';
import { router, promptAction } from '@kit.ArkUI';
import { Logger } from '../common/Logger';
import { formatBytes } from '../common/Util';
import { VpnConfig } from '../model/VpnConfig';
import { AppSession } from '../service/AppSession';
import { StorageService } from '../service/StorageService';
import { DownloadProgress, RemotePackage, UpdateException, UpdateService } from '../service/UpdateService';

const log = new Logger('index');

/** 列表项长按菜单动作. */
class MenuAction {
  label: string = '';
  run: () => void = () => {
  };

  constructor(label: string, run: () => void) {
    this.label = label;
    this.run = run;
  }
}

@Entry
@Component
struct Index {
  @State configs: VpnConfig[] = [];
  @State runningId: string = '';
  @State busy: boolean = false;
  @State updateMessage: string = '';
  @State downloading: boolean = false;
  @State progress: number = 0;
  @State progressText: string = '';
  @State showUpdatePanel: boolean = false;
  private service: StorageService | undefined = undefined;
  private updater: UpdateService | undefined = undefined;
  private pendingPackage: RemotePackage | undefined = undefined;
  private downloadedPath: string = '';

  aboutToAppear(): void {
    const context = this.getUIContext().getHostContext() as common.UIAbilityContext;
    this.service = new StorageService(context);
    this.updater = new UpdateService(context);
    this.refresh();
    AppSession.instance.subscribe(() => {
      this.runningId = AppSession.instance.runningConfigId;
    });
    this.autoCheckUpdate();
  }

  onPageShow(): void {
    this.refresh();
  }

  private refresh(): void {
    const service = this.service;
    if (service !== undefined) {
      this.configs = service.loadConfigs();
    }
    this.runningId = AppSession.instance.runningConfigId;
  }

  private save(): void {
    const service = this.service;
    if (service !== undefined) {
      service.saveConfigs(this.configs);
    }
  }

  /* ------------------------------ 更新检查 ------------------------------ */

  private async autoCheckUpdate(): Promise<void> {
    const updater = this.updater;
    if (updater === undefined || !updater.isCheckDue()) {
      return;
    }
    try {
      const remote: RemotePackage = await updater.fetchRemote();
      updater.saveLastCheck();
      if (remote.hash === updater.handledHash()) {
        return;
      }
      this.pendingPackage = remote;
      this.updateMessage = `发现新版本 (${formatBytes(remote.size)})`;
      this.showUpdatePanel = true;
    } catch (err) {
      log.warn(`自动检查更新失败: ${(err as Error).message}`);
    }
  }

  private async checkUpdateManually(): Promise<void> {
    const updater = this.updater;
    if (updater === undefined || this.busy) {
      return;
    }
    this.busy = true;
    try {
      const remote: RemotePackage = await updater.fetchRemote();
      updater.saveLastCheck();
      if (remote.hash === updater.handledHash()) {
        promptAction.showToast({ message: '已是最新版本' });
        return;
      }
      this.pendingPackage = remote;
      this.updateMessage = `发现新版本 (${formatBytes(remote.size)})`;
      this.showUpdatePanel = true;
    } catch (err) {
      promptAction.showToast({ message: (err as Error).message });
    } finally {
      this.busy = false;
    }
  }

  private async startDownload(): Promise<void> {
    const updater = this.updater;
    if (updater === undefined || this.downloading) {
      return;
    }
    this.downloading = true;
    this.progress = 0;
    this.progressText = '开始下载';
    try {
      this.downloadedPath = await updater.download((progress: DownloadProgress) => {
        this.progress = progress.total > 0 ? progress.received / progress.total : 0;
        this.progressText = progress.total > 0
          ? `${formatBytes(progress.received)} / ${formatBytes(progress.total)}`
          : formatBytes(progress.received);
      });
      this.progressText = '下载完成, 正在调起安装';
      const pkg = this.pendingPackage;
      if (pkg !== undefined) {
        updater.saveHandledHash(pkg.hash);
      }
      await updater.install(this.downloadedPath);
      this.downloading = false;
      this.showUpdatePanel = false;
    } catch (err) {
      this.downloading = false;
      const message: string = err instanceof UpdateException ? (err as Error).message : (err as Error).message;
      promptAction.showToast({ message: message });
    }
  }

  /* ------------------------------ 配置操作 ------------------------------ */

  private async runConfig(config: VpnConfig): Promise<void> {
    const errors: string[] = config.validate();
    if (errors.length > 0) {
      promptAction.showToast({ message: errors[0] });
      return;
    }
    const context = this.getUIContext().getHostContext() as common.UIAbilityContext;
    this.busy = true;
    try {
      if (AppSession.instance.isRunning()) {
        await AppSession.instance.stop(context);
      }
      await AppSession.instance.start(context, config);
      this.runningId = config.id;
      router.pushUrl({ url: 'pages/RunningPage' });
    } catch (err) {
      promptAction.showToast({ message: `启动失败: ${(err as Error).message}` });
    } finally {
      this.busy = false;
    }
  }

  private duplicate(config: VpnConfig): void {
    const copy: VpnConfig = config.copy();
    copy.id = VpnConfig.create(config.name).id;
    copy.name = `${config.name} 副本`;
    this.configs.push(copy);
    this.save();
    this.refresh();
  }

  private remove(config: VpnConfig): void {
    AlertDialog.show({
      title: '删除配置',
      message: `确定删除「${config.name}」?`,
      primaryButton: {
        value: '取消',
        action: () => {
        }
      },
      secondaryButton: {
        value: '删除',
        fontColor: Color.Red,
        action: () => {
          const index: number = this.configs.findIndex((item: VpnConfig) => item.id === config.id);
          if (index >= 0) {
            this.configs.splice(index, 1);
            this.save();
            this.refresh();
          }
        }
      }
    });
  }

  private menuActions(config: VpnConfig): MenuAction[] {
    const actions: MenuAction[] = [];
    actions.push(new MenuAction('运行', () => {
      this.runConfig(config);
    }));
    actions.push(new MenuAction('编辑', () => {
      router.pushUrl({ url: 'pages/ConfigEditPage', params: { id: config.id } });
    }));
    actions.push(new MenuAction('分享二维码', () => {
      router.pushUrl({ url: 'pages/SharePage', params: { id: config.id } });
    }));
    actions.push(new MenuAction('复制', () => {
      this.duplicate(config);
    }));
    actions.push(new MenuAction('删除', () => {
      this.remove(config);
    }));
    return actions;
  }

  @Builder
  configItem(config: VpnConfig) {
    Column() {
      Row() {
        Text(config.name)
          .fontSize(18)
          .fontWeight(FontWeight.Medium)
          .layoutWeight(1)
        if (config.id === this.runningId) {
          Text('运行中')
            .fontSize(12)
            .fontColor(Color.White)
            .backgroundColor('#2E7D32')
            .padding({ left: 8, right: 8, top: 2, bottom: 2 })
        }
      }
      .width('100%')

      Text(config.maskedProxyPass().length > 0 ? config.maskedProxyPass() : '未填写代理地址')
        .fontSize(14)
        .fontColor('#666666')
        .margin({ top: 4 })
        .width('100%')
        .maxLines(1)
        .textOverflow({ overflow: TextOverflow.Ellipsis })

      Text(this.describe(config))
        .fontSize(12)
        .fontColor('#999999')
        .margin({ top: 4 })
        .width('100%')
    }
    .width('100%')
    .padding(14)
    .backgroundColor(Color.White)
    .borderRadius(0)
    .onClick(() => {
      router.pushUrl({ url: 'pages/ConfigEditPage', params: { id: config.id } });
    })
    .gesture(LongPressGesture().onAction(() => {
      this.getUIContext().showActionSheet({
        title: config.name,
        message: this.describe(config),
        sheets: this.sheetsFor(config)
      });
    }))
  }

  private sheetsFor(config: VpnConfig): SheetInfo[] {
    const sheets: SheetInfo[] = [];
    const actions: MenuAction[] = this.menuActions(config);
    for (const action of actions) {
      sheets.push({
        title: action.label,
        action: () => {
          action.run();
        }
      });
    }
    return sheets;
  }

  private describe(config: VpnConfig): string {
    const parts: string[] = [];
    if (config.globalProxy) {
      parts.push('全局代理');
    } else {
      parts.push(`域名 ${config.proxyDomains.length}`);
      parts.push(`CIDR ${config.proxyCidr.length}`);
    }
    parts.push(`MTU ${config.tunMtu}`);
    if (config.bypassCn) {
      parts.push('绕过中国大陆');
    }
    return parts.join(' · ');
  }

  @Builder
  updatePanel() {
    Column() {
      Text(this.updateMessage)
        .fontSize(16)
        .width('100%')
      if (this.downloading) {
        Progress({ value: this.progress * 100, total: 100, type: ProgressType.Linear })
          .width('100%')
          .margin({ top: 12 })
        Text(this.progressText)
          .fontSize(12)
          .fontColor('#666666')
          .margin({ top: 6 })
          .width('100%')
      } else {
        Row() {
          Button('稍后')
            .layoutWeight(1)
            .onClick(() => {
              this.showUpdatePanel = false;
            })
          Button('下载并安装')
            .layoutWeight(1)
            .margin({ left: 12 })
            .onClick(() => {
              this.startDownload();
            })
        }
        .width('100%')
        .margin({ top: 16 })
      }
    }
    .width('90%')
    .padding(20)
    .backgroundColor(Color.White)
    .borderRadius(0)
  }

  build() {
    Stack() {
      Column() {
        Row() {
          Text('xProxy')
            .fontSize(22)
            .fontWeight(FontWeight.Bold)
            .layoutWeight(1)
          Text('扫码')
            .fontSize(16)
            .padding(6)
            .onClick(() => {
              router.pushUrl({ url: 'pages/ScanPage' });
            })
          Text('更新')
            .fontSize(16)
            .padding(6)
            .margin({ left: 8 })
            .onClick(() => {
              this.checkUpdateManually();
            })
        }
        .width('100%')
        .padding({ left: 16, right: 16, top: 12, bottom: 12 })

        if (this.runningId.length > 0) {
          Row() {
            Text('VPN 运行中, 点击查看状态')
              .fontSize(14)
              .fontColor(Color.White)
              .layoutWeight(1)
            Text('查看')
              .fontSize(14)
              .fontColor(Color.White)
          }
          .width('100%')
          .padding(12)
          .backgroundColor('#2E7D32')
          .onClick(() => {
            router.pushUrl({ url: 'pages/RunningPage' });
          })
        }

        if (this.configs.length === 0) {
          Column() {
            Text('还没有配置')
              .fontSize(16)
              .fontColor('#999999')
            Text('点击右下角新建, 或从二维码导入')
              .fontSize(13)
              .fontColor('#BBBBBB')
              .margin({ top: 8 })
          }
          .layoutWeight(1)
          .width('100%')
          .justifyContent(FlexAlign.Center)
        } else {
          List({ space: 8 }) {
            ForEach(this.configs, (config: VpnConfig) => {
              ListItem() {
                this.configItem(config)
              }
            }, (config: VpnConfig) => config.id)
          }
          .layoutWeight(1)
          .width('100%')
          .padding({ left: 12, right: 12 })
        }

        Button('新建配置')
          .width('100%')
          .margin({ left: 12, right: 12, bottom: 16 })
          .onClick(() => {
            router.pushUrl({ url: 'pages/ConfigEditPage', params: { id: '' } });
          })
      }
      .width('100%')
      .height('100%')
      .backgroundColor('#F1F3F5')

      if (this.showUpdatePanel) {
        Column() {
          this.updatePanel()
        }
        .width('100%')
        .height('100%')
        .backgroundColor('rgba(0, 0, 0, 0.35)')
        .justifyContent(FlexAlign.Center)
        .alignItems(HorizontalAlign.Center)
      }
    }
    .width('100%')
    .height('100%')
  }
}
