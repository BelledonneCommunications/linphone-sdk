BASE_NAME=gitlab.linphone.org:4567/bc/public/linphone-sdk

android-r16b:
	docker build -f bc-dev-android-r16b -t $(BASE_NAME)/bc-dev-android:r16b --rm .

android-r17c:
	docker build -f bc-dev-android-r17c -t $(BASE_NAME)/bc-dev-android:r17c --rm .

archlinux-latest:
	docker build -f bc-dev-archlinux-latest -t $(BASE_NAME)/bc-dev-archlinux:latest --rm .

centos7:
	docker build -f bc-dev-centos7 -t $(BASE_NAME)/bc-dev-centos:7 --rm .

debian8:
	docker build -f bc-dev-debian8 -t $(BASE_NAME)/bc-dev-debian:8 --rm .

debian9:
	docker build -f bc-dev-debian9 -t $(BASE_NAME)/bc-dev-debian:9 --rm .

ubuntu-rolling:
	docker build -f bc-dev-ubuntu-rolling -t $(BASE_NAME)/bc-dev-ubuntu:rolling --rm .

.PHONY: android-r16b android-r17c archlinux-latest centos7 debian8 debian9 ubuntu-rolling
