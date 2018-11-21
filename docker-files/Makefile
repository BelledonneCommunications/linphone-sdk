BASE_NAME=gitlab.linphone.org:4567/bc/public/linphone-sdk

centos7:
	docker build -f bc-dev-centos7 -t $(BASE_NAME)/bc-dev-centos:7 --rm .

debian8:
	docker build -f bc-dev-debian8 -t $(BASE_NAME)/bc-dev-debian:8 --rm .

debian9:
	docker build -f bc-dev-debian9 -t $(BASE_NAME)/bc-dev-debian:9 --rm .

.PHONY: centos7 debian8 debian9
