all: restored_banana

restored_banana: clean
	@echo Compiling…;
	@xcrun -sdk iphoneos clang --sysroot /Applications/Xcode.app/Contents/Developer/Platforms/iPhoneOS.platform/Developer/SDKs/iPhoneOS.sdk restored_banana.c -arch arm64 -framework IOKit -framework CoreFoundation -framework IOSurface -Wall -I. -o restored_banana;
	@echo Signing…;
	@ldid -Sent.plist restored_banana;
	@echo Built restored_banana;
	@scp -P2222 restored_banana root@localhost:/mnt1/private/var/root/banana;

clean:
	@rm -f restored_banana;