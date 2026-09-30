import { AbilityConstant, UIAbility, Want } from '@kit.AbilityKit';
import { window } from '@kit.ArkUI';
import { Logger } from '../common/Logger';
import { AppSession } from '../service/AppSession';

const log = new Logger('entry');

export default class EntryAbility extends UIAbility {
  onCreate(want: Want, launchParam: AbilityConstant.LaunchParam): void {
    log.info('EntryAbility onCreate');
  }

  onDestroy(): void {
    log.info('EntryAbility onDestroy');
  }

  onWindowStageCreate(windowStage: window.WindowStage): void {
    windowStage.loadContent('pages/Index', (err) => {
      if (err.code) {
        log.error(`loadContent failed: ${JSON.stringify(err)}`);
      }
    });
  }

  onWindowStageDestroy(): void {
  }

  onForeground(): void {
    // 进程重启后恢复仍在运行的 VPN 控制通道.
    AppSession.instance.restore(this.context).catch((err: Error) => {
      log.warn(`恢复运行状态失败: ${err.message}`);
    });
  }

  onBackground(): void {
  }
}
