BASE_NAME=gitlab.linphone.org:4567/bc/public/linphone-sdk

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

debian9:
	docker build -f bc-dev-debian9 -t $(BASE_NAME)/bc-dev-debian:9 --rm .

ubuntu-rolling:
	docker build -f bc-dev-ubuntu-rolling -t $(BASE_NAME)/bc-dev-ubuntu:rolling --rm .

ubuntu-18.04-LTS:
	docker build -f bc-dev-ubuntu-18.04-LTS -t $(BASE_NAME)/bc-dev-ubuntu:18.04 --rm .

.PHONY: archlinux-latest centos7 centos7-mediastreamer centos7-lime centos7-liblinphone debian9 ubuntu-rolling ubuntu-18.04-LTS
