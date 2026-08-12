import { toast } from "sonner";

export type ToastType = "ok" | "err" | "warn" | "info";

// 轻提示：ok（绿）/ err（红）/ warn（黄）/ info（默认）。
export function showToast(msg: string, type: ToastType = "info") {
  switch (type) {
    case "ok":
      toast.success(msg);
      break;
    case "err":
      toast.error(msg);
      break;
    case "warn":
      toast.warning(msg);
      break;
    default:
      toast(msg);
  }
}
