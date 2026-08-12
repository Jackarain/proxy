var __defProp = Object.defineProperty;
var __name = (target, value) => __defProp(target, "name", { value, configurable: true });

// src/use-rect.tsx
import * as React from "react";
import { observeElementRect } from "@radix-ui/rect";
function useRect(measurable) {
  const [rect, setRect] = React.useState();
  React.useEffect(() => {
    if (measurable) {
      const unobserve = observeElementRect(measurable, setRect);
      return () => {
        setRect(void 0);
        unobserve();
      };
    }
    return;
  }, [measurable]);
  return rect;
}
__name(useRect, "useRect");
export {
  useRect
};
//# sourceMappingURL=index.mjs.map
