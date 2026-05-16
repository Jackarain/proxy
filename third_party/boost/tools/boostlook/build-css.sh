#!/bin/sh
# Concatenate CSS modules into boostlook-v3.css
cat \
  src/css/00-header.css \
  src/css/01-variables.css \
  src/css/02-themes.css \
  src/css/03-fonts.css \
  src/css/04-reset.css \
  src/css/05-global-typography.css \
  src/css/06-global-links.css \
  src/css/07-global-code.css \
  src/css/08-global-components.css \
  src/css/09-global-tables-images.css \
  src/css/10-scrollbars.css \
  src/css/11-template-layout.css \
  src/css/12-asciidoctor.css \
  src/css/13-antora.css \
  src/css/14-quickbook.css \
  src/css/15-readme.css \
  src/css/16-responsive-toc.css \
  > boostlook-v3.css
