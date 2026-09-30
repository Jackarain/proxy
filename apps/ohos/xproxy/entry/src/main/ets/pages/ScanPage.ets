import { common, abilityAccessCtrl, Permissions } from '@kit.AbilityKit';
import { router, promptAction } from '@kit.ArkUI';
import { customScan, scanBarcode, scanCore } from '@kit.ScanKit';
import { VpnConfig } from '../model/VpnConfig';
import { decodeConfigShare } from '../service/ConfigShare';
import { StorageService } from '../service/StorageService';
import { Logger } from '../common/Logger';

const log = new Logger('scan');
const CAMERA: Permissions = 'ohos.permission.CAMERA';

/** 扫码导入配置页. */
@Entry
@Component
struct ScanPage {
  @State message: string = '将配置二维码放入取景框';
  @State scanning: boolean = false;
  private controller: XComponentController = new XComponentController();
  private handled: boolean = false;

  async aboutToAppear(): Promise<void> {
    const context = this.getUIContext().getHostContext() as common.UIAbilityContext;
    try {
      const result = await abilityAccessCtrl.createAtManager()
        .requestPermissionsFromUser(context, [CAMERA]);
      if (result.authResults.length === 0 || result.authResults[0] !== 0) {
        this.message = '未授予相机权限';
      }
    } catch (err) {
      this.message = `相机权限申请失败: ${(err as Error).message}`;
    }
  }

  private startScan(): void {
    if (this.scanning) {
      return;
    }
    this.scanning = true;
    try {
      customScan.init({ scanTypes: [scanCore.ScanType.QR_CODE] });
      customScan.start({
        width: 1080,
        height: 1440,
        surfaceId: this.controller.getXComponentSurfaceId()
      }).then((results: scanBarcode.ScanResult[]) => {
        this.scanning = false;
        this.onScanResult(results);
      }).catch((err: Error) => {
        this.scanning = false;
        this.message = `扫码失败: ${err.message}`;
        log.warn(`customScan 失败: ${err.message}`);
      });
    } catch (err) {
      this.scanning = false;
      this.message = `扫码启动失败: ${(err as Error).message}`;
    }
  }

  private onScanResult(results: scanBarcode.ScanResult[]): void {
    if (this.handled || results.length === 0) {
      return;
    }
    const content: string = results[0].originalValue;
    if (content.length === 0) {
      return;
    }
    this.handled = true;
    try {
      const config: VpnConfig = decodeConfigShare(content);
      config.id = VpnConfig.create(config.name).id;
      const context = this.getUIContext().getHostContext() as common.UIAbilityContext;
      const service = new StorageService(context);
      const configs: VpnConfig[] = service.loadConfigs();
      configs.push(config);
      service.saveConfigs(configs);
      promptAction.showToast({ message: `已导入「${config.name}」` });
      router.back();
    } catch (err) {
      this.handled = false;
      this.message = (err as Error).message;
      customScan.rescan();
    }
  }

  aboutToDisappear(): void {
    try {
      customScan.stop();
      customScan.release();
    } catch (err) {
      log.warn(`释放扫码资源失败: ${(err as Error).message}`);
    }
  }

  build() {
    Stack() {
      XComponent({
        id: 'scanView',
        type: XComponentType.SURFACE,
        controller: this.controller
      })
        .width('100%')
        .height('100%')
        .onLoad(() => {
          this.startScan();
        })

      Column() {
        Row() {
          Text('返回')
            .fontSize(16)
            .fontColor(Color.White)
            .onClick(() => {
              router.back();
            })
        }
        .width('100%')
        .padding(12)

        Text(this.message)
          .fontSize(14)
          .fontColor(Color.White)
          .backgroundColor('rgba(0,0,0,0.5)')
          .padding(10)
          .margin({ top: 12 })
      }
      .width('100%')
      .height('100%')
      .alignItems(HorizontalAlign.Center)
    }
    .width('100%')
    .height('100%')
    .backgroundColor(Color.Black)
  }
}
