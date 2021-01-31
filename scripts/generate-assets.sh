#!/bin/bash

## Copyright (C) 2004-2021 Robert Griebl. All rights reserved.
##
## This file is part of BrickStore.
##
## This file may be distributed and/or modified under the terms of the GNU 
## General Public License version 2 as published by the Free Software Foundation 
## and appearing in the file LICENSE.GPL included in the packaging of this file.
##
## This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE
## WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
##
## See http://fsf.org/licensing/licenses/gpl.html for GPL licensing information.

#set +x

b="$(dirname $0)/../assets"
which realpath >/dev/null && b="$(realpath --relative-to=. $b)"

cus="$b/custom"

gai="$b/generated-app-icons"
gin="$b/generated-installers"

theme="brickstore-breeze"
s=64

mkdir -p "$gai"
mkdir -p "$gin"

######################################
# app and doc icons

echo -n "Generating app and doc icons..."

# Unix icons
convert $b/brickstore.png -resize 256 $gai/brickstore.png
convert -size 128x128 canvas:transparent \
        $cus/crystal-clear-spreadsheet.png -composite \
        $b/brickstore.png -geometry 88x88+20+4 -composite \
        $gai/brickstore_doc.png

# Windows icons
convert $gai/brickstore.png -define icon:auto-resize=256,48,32,16 $gai/brickstore.ico
convert $gai/brickstore_doc.png -define icon:auto-resize=128,48,32,16 $gai/brickstore_doc.ico

# macOS icons
## png2icns is broken for icons >= 256x256
#png2icns $gai/brickstore.icns $gai/brickstore.png >/dev/null
#png2icns $gai/brickstore_doc.icns $gai/brickstore_doc.png >/dev/null

## and makeicns is only available on macOS via brew
if which makeicns >/dev/null; then
  makeicns -256 $gai/brickstore.png -32 $gai/brickstore.png -out $gai/brickstore.icns
  makeicns -128 $gai/brickstore_doc.png -32 $gai/brickstore_doc.png -out $gai/brickstore_doc.icns
fi

echo "done"

#######################################
# edit icons with overlays

echo -n "Generating action icons..."

tmp="$b/tmp"
mkdir -p "$tmp"

for color in "" "-dark"; do

  rsvg-convert $b/custom/brick-1x1$color.svg -w $s -h $s -f png -o $tmp/brick-1x1$color.png
  rsvg-convert $b/icons/$theme$color/svg/taxes-finances.svg -w $s -h $s -f png -o $tmp/dollar$color.png
  rsvg-convert $b/icons/$theme$color/svg/help-about.svg -w $s -h $s -f png -o $tmp/info$color.png

  out="$b/icons/$theme$color/generated"
  mkdir -p "$out"

  convert $tmp/brick-1x1$color.png -colorspace sRGB -scale $s $cus/overlay_plus.png -scale $s -composite $out/edit-additems.png
  convert $tmp/brick-1x1$color.png -colorspace sRGB -scale $s $cus/overlay_divide.png -scale $s -composite $out/edit-qty-divide.png
  convert $tmp/brick-1x1$color.png -colorspace sRGB -scale $s $cus/overlay_multiply.png -scale $s -composite $out/edit-qty-multiply.png
  convert $tmp/brick-1x1$color.png -colorspace sRGB -scale $s $cus/overlay_minus.png -scale $s -composite $out/edit-subtractitems.png
  convert $tmp/brick-1x1$color.png -colorspace sRGB -scale $s $cus/overlay_merge.png -scale $s -composite $out/edit-mergeitems.png
  convert $tmp/brick-1x1$color.png -colorspace sRGB -scale $s $cus/overlay_split.png -scale $s -composite $out/edit-partoutitems.png

  convert $tmp/dollar$color.png -colorspace sRGB -scale $s $cus/overlay_plusminus.png -scale $s -composite $out/edit-price-inc-dec.png
  convert $tmp/dollar$color.png -colorspace sRGB -scale $s $cus/overlay_equals.png -scale $s -composite $out/edit-price-set.png
  convert $tmp/dollar$color.png -colorspace sRGB -scale $s $cus/overlay_percent.png -scale $s -composite $out/edit-sale.png

  convert -size ${s}x${s} canvas:transparent \
        \( $tmp/dollar$color.png -scale $s \) -geometry +0+0 -composite \
        \( $cus/bricklink.png -scale $((s*5/8)) \) -geometry +$((s*3/8))+$((s*3/8)) -composite \
        $out/edit-price-to-priceguide.png

  convert -size ${s}x${s} canvas:transparent \
        \( $cus/bricklink.png -scale $((s*5/8)) \) -geometry +0+0 -composite \
        \( $tmp/dollar$color.png -scale $((s*5/8)) \) -geometry +$((s*3/8))+$((s*3/8)) -composite \
        $out/bricklink-priceguide.png

  convert -size ${s}x${s} canvas:transparent \
        \( $cus/bricklink.png -scale $((s*5/8)) \) -geometry +0+0 -composite \
        \( $tmp/info$color.png -scale $((s*5/8)) \) -geometry +$((s*3/8))+$((s*3/8)) -composite \
        $out/bricklink-catalog.png

  convert -size ${s}x${s} canvas:transparent \
        \( $cus/bricklink.png -scale $((s*5/8)) \) -geometry +0+0 -composite \
        \( $tmp/brick-1x1$color.png -scale $((s*5/8)) \) -geometry +$((s*3/8))+$((s*3/8)) -composite \
        $out/bricklink-lotsforsale.png
done

rm -rf "$tmp"

echo "done"

#######################################
# creating installer images

echo "Generating images for installers..."

convert $gai/brickstore.png -resize 96x96 -define bmp3:alpha=true bmp3:$gin/windows-installer.bmp

echo "done"


#######################################
# optimize sizes

if which zopflipng >/dev/null; then
  echo "Optimizing..."

  for png in $(ls -1 $cus/*.png $gai/*.png $gin/*.png $b/icons/$theme/generated $b/icons/${theme}-dark/generated); do
    echo -n " > ${png}... "
    zopflipng -my "$png" "$png" >/dev/null
    #optipng -o7 "$1"
    echo "done"
  done
else
  echo "Not optimizing: zopflipng is not available"
fi
