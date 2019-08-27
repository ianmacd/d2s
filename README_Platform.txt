How to build Module for Platform
- It is only for modules are needed to using Android build system.
- Please check its own install information under its folder for other module.

[Step to build]
1. Get android open source.
    : version info - Android 9.0
    ( Download site : http://source.android.com )

2. Copy module that you want to build - to original android open source
   If same module exist in android open source, you should replace it. (no overwrite)
   
  # It is possible to build all modules at once.
  
3. You should add module name to 'PRODUCT_PACKAGES' in 'build\target\product\core.mk' as following case.
	case 1) e2fsprog : should add 'e2fsck','resize2fs' to PRODUCT_PACKAGES
	case 2) libexifa : should add 'libexifa.camera.samsung' to PRODUCT_PACKAGES
	case 3) libjpega : should add 'libjpega.camera.samsung' to PRODUCT_PACKAGES
	case 4) keyutils : should add 'libknox_keyutils' to PRODUCT_PACKAGES
	

ex.) [build\target\product\core.mk] - add all module name for case 1 ~ 5 at once
    
# e2fsprog
PRODUCT_PACKAGES += \
    e2fsck \
    libext2fs \
    libext2_blkid \
    ibext2_e2p \
    resize2fs
    
# libexifa
PRODUCT_PACKAGES += \
    libexifa.camera.samsung
    
# libjpega
PRODUCT_PACKAGES += \
    libjpega.camera.samsung
    
# KeyUtils
PRODUCT_PACKAGES += \
    libknox_keyutils
   
4. excute build command
   ./build_64bit.sh

5. Note : 
   To download the source code of S/W listed below, please visit http://opensource.samsung.com and find ¡°Mobile -> Mobile Application¡± menu, 
   and then, you will be able to download what you want. 
   You might save time in finding the right one by making use of the search keyword below. 

   - Notes31.apk : "Samsung Notes"
   - MdecService : "MdecService"
   - SmartSwitch.apk : "Smart Switch" (Version : 3.4)            
   - SamsungMessages_10.0.apk : "Messaging"
   - SecEmail_P.apk : "Email"
   - SBrowser_10.0_Removable.apk : "SBrowser" (version : SBrowser v10)      
   - SamsungConnect.apk : "SamsungConnect"
   - KidsHome_Installer.apk : "KidsHome"
   - SamsungCamera.apk : "Camera8"
   - AREmoji.apk : "AREmoji"
   - Fmm.apk : "FMM"   
   - BixbyVisionFramework3.apk: "BixbyVisionFramework"
   - VoiceNote_5.0.apk : "Voice Recorder"
   - DeXonPC.apk : "DeXonPC"
   - LightweightWebEngine.apk : "lightweight-web-engine"