#!/bin/bash

PACKAGE_NAME="IFWFilterWheel_X2.pkg"
BUNDLE_NAME="org.rti-zone.IFWFilterWheelX2"

if [ ! -z "$app_id_signature" ]; then
    codesign -f -s "$app_id_signature" --verbose ../build/Release/libIFWFilterWheel.dylib
fi

mkdir -p ROOT/tmp/IFWFilterWheel_X2/
cp "../IFW.ui" ROOT/tmp/IFWFilterWheel_X2/
cp "../filterwheellist optec.txt" ROOT/tmp/IFWFilterWheel_X2/
cp "../build/Release/libIFWFilterWheel.dylib" ROOT/tmp/IFWFilterWheel_X2/


if [ ! -z "$installer_signature" ]; then
	# signed package using env variable installer_signature
	pkgbuild --root ROOT --identifier $BUNDLE_NAME --sign "$installer_signature" --scripts Scripts --version 1.0 $PACKAGE_NAME
	pkgutil --check-signature ./${PACKAGE_NAME}
else
	pkgbuild --root ROOT --identifier $BUNDLE_NAME --scripts Scripts --version 1.0 $PACKAGE_NAME
fi

rm -rf ROOT
