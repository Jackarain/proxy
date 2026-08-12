"use client";
var __defProp = Object.defineProperty;
var __name = (target, value) => __defProp(target, "name", { value, configurable: true });

// src/direction.tsx
import * as React from "react";
import { jsx } from "react/jsx-runtime";
var DirectionContext = React.createContext(void 0);
var DirectionProvider = /* @__PURE__ */ __name((props) => {
  const { dir, children } = props;
  return /* @__PURE__ */ jsx(DirectionContext.Provider, { value: dir, children });
}, "DirectionProvider");
function useDirection(localDir) {
  const globalDir = React.useContext(DirectionContext);
  return localDir || globalDir || "ltr";
}
__name(useDirection, "useDirection");
var Provider = DirectionProvider;
export {
  DirectionProvider,
  Provider,
  useDirection
};
//# sourceMappingURL=index.mjs.map
