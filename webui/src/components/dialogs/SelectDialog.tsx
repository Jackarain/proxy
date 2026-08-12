import {
  Dialog,
  DialogContent,
  DialogFooter,
  DialogHeader,
  DialogTitle,
} from "@/components/ui/dialog";
import { Button } from "@/components/ui/button";
import { useDialogs } from "@/store/dialogs";

// 选项列表弹窗：按钮式选项列表，点某项返回该值，取消返回 null。
export default function SelectDialog() {
  const select = useDialogs((s) => s.select);
  const closeSelect = useDialogs((s) => s.closeSelect);
  const open = !!select;

  return (
    <Dialog open={open} onOpenChange={(o) => !o && closeSelect(null)}>
      <DialogContent className="w-[320px] max-w-[92vw]">
        <DialogHeader>
          <DialogTitle>{select?.title ?? ""}</DialogTitle>
        </DialogHeader>
        <div className="flex flex-col gap-1.5">
          {select?.options.map((opt) => (
            <Button
              key={opt.value}
              className="w-full"
              onClick={() => closeSelect(opt.value)}
            >
              {opt.label}
            </Button>
          ))}
        </div>
        <DialogFooter>
          <Button onClick={() => closeSelect(null)}>取消</Button>
        </DialogFooter>
      </DialogContent>
    </Dialog>
  );
}
