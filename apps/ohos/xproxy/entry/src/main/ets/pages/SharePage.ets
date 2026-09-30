import { common } from '@kit.AbilityKit';
import { router } from '@kit.ArkUI';
import { image } from '@kit.ImageKit';
import { generateBarcode, scanCore } from '@kit.ScanKit';
import { VpnConfig } from '../model/VpnConfig';
import { encodeConfigShare } from '../service/ConfigShare';
import { StorageService } from '../service/StorageService';
import { Logger } from '../common/Logger';

const log = new Logger('share');

interface ShareParams {
  id?: string;
}

/** 配置分享二维码页. */
@Entry
@Component
struct SharePage {
  @State name: string = '';
  @State message: string = '正在生成二维码';
  @State pixelMap: image.PixelMap | undefined = undefined;

  aboutToAppear(): void {
    const context = this.getUIContext().getHostContext() as common.UIAbilityContext;
    const params = router.getParams() as ShareParams;
    const id: string = params !== undefined && params.id !== undefined ? params.id : '';
    const config: VpnConfig | undefined = new StorageService(context).loadConfigs()
      .find((item: VpnConfig) => item.id === id);
    if (config === undefined) {
      this.message = '配置不存在';
      return;
    }
    this.name = config.name;
    this.renderQr(encodeConfigShare(config));
  }

  private renderQr(content: string): void {
    generateBarcode.createBarcode(content, {
      scanType: scanCore.ScanType.QR_CODE,
      height: 720,
      width: 720,
      margin: 1
    }).then((pixelMap: image.PixelMap) => {
      this.pixelMap = pixelMap;
      this.message = '';
    }).catch((err: Error) => {
      this.message = `二维码生成失败: ${err.message}`;
      log.error(`生成二维码失败: ${err.message}`);
    });
  }

  build() {
    Column() {
      Row() {
        Text('返回')
          .fontSize(16)
          .onClick(() => {
            router.back();
          })
        Text(this.name)
          .fontSize(20)
          .fontWeight(FontWeight.Bold)
          .layoutWeight(1)
          .textAlign(TextAlign.Center)
        Text('')
          .fontSize(16)
          .width(32)
      }
      .width('100%')
      .padding(12)

      Column() {
        if (this.pixelMap !== undefined) {
          Image(this.pixelMap)
            .width(280)
            .height(280)
            .interpolation(ImageInterpolation.None)
        } else {
          Text(this.message)
            .fontSize(14)
            .fontColor('#666666')
        }
      }
      .layoutWeight(1)
      .width('100%')
      .justifyContent(FlexAlign.Center)

      Text('扫码即可导入本配置 (分流列表仅保留前 10 条)')
        .fontSize(12)
        .fontColor('#999999')
        .width('100%')
        .textAlign(TextAlign.Center)
        .padding(16)
    }
    .width('100%')
    .height('100%')
    .backgroundColor(Color.White)
  }
}
