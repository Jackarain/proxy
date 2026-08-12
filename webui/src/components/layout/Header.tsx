import { Plus } from "lucide-react";
import { useApp } from "@/store/app";
import { useDialogs } from "@/store/dialogs";
import { Button } from "@/components/ui/button";

export default function Header() {
  const version = useApp((s) => s.version);
  const openNew = useDialogs((s) => s.openNew);

  return (
    <header className="flex shrink-0 items-center justify-between border-b border-border bg-card px-5 py-2.5">
      <div className="flex items-center gap-2.5">
        <div
          title="Cproxy"
          className="flex h-8 w-8 shrink-0 items-center justify-center bg-primary text-lg font-black text-primary-foreground"
        >
          C
        </div>
        <div className="text-xl font-bold">
          Cproxy <span className="text-primary">launcher</span>
          {version && (
            <span className="ml-2 text-xs font-normal text-muted-foreground">
              {version}
            </span>
          )}
        </div>
      </div>
      <Button variant="primary" onClick={openNew}>
        <Plus />
        新建实例
      </Button>
    </header>
  );
}
