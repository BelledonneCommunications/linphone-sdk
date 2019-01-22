BASE_NAME=gitlab.linphone.org:4567/bc/public/linphone-sdk

android-r16b:
	docker build -f bc-dev-android-r16b -t $(BASE_NAME)/bc-dev-android:r16b --rm .

android-r17c:
	docker build -f bc-dev-android-r17c -t $(BASE_NAME)/bc-dev-android:r17c --rm .

android-r18b:
	docker build -f bc-dev-android-r18b -t $(BASE_NAME)/bc-dev-android:r18b --rm .

android-r19:
	docker build -f bc-dev-android-r19 -t $(BASE_NAME)/bc-dev-android:r19 --rm .

archlinux-latest:
	docker build -f bc-dev-archlinux-latest -t $(BASE_NAME)/bc-dev-archlinux:latest --rm .

centos7:
	docker build -f bc-dev-centos7 -t $(BASE_NAME)/bc-dev-centos:7 --rm .

centos7-mediastreamer:
	docker build -f bc-dev-centos7-test-mediastreamer -t $(BASE_NAME)/bc-dev-centos-test-mediastreamer:7 --rm .

centos7-liblinphone:
	docker build -f bc-dev-centos7-test-liblinphone -t $(BASE_NAME)/bc-dev-centos-test-liblinphone:7 --rm .

centos7-lime:
	docker build -f bc-dev-centos7-test-lime -t $(BASE_NAME)/bc-dev-centos-test-lime:7 --rm .

debian8:
	docker build -f bc-dev-debian8 -t $(BASE_NAME)/bc-dev-debian:8 --rm .

debian9:
	docker build -f bc-dev-debian9 -t $(BASE_NAME)/bc-dev-debian:9 --rm .

ubuntu-rolling:
	docker build -f bc-dev-ubuntu-rolling -t $(BASE_NAME)/bc-dev-ubuntu:rolling --rm .

ubuntu-18.04-LTS:
	docker build -f bc-dev-ubuntu-18.04-LTS -t $(BASE_NAME)/bc-dev-ubuntu:18.04 --rm .

.PHONY: android-r16b android-r17c android-r18b android-r19 archlinux-latest centos7 centos7-mediastreamer centos7-lime centos7-liblinphone debian8 debian9 ubuntu-rolling ubuntu-18.04-LTS
