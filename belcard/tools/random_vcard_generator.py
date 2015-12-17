#!/usr/bin/env python
# -*- coding: utf8 -*-

from barnum import gen_data
import codecs
import frogress
import random

gender_vcard_list = ['M', 'F', 'O', 'N', 'U']

def generate_vCard():
	gender_initial = gender_vcard_list[random.randint(0, 4)]
	gender = None
	if gender_initial == 'M':
		gender = 'Male'
	elif gender_initial == 'F':
		gender = 'Female'

	(first_name, last_name) = gen_data.create_name(gender=gender)
	adr = gen_data.create_street()
	zip, city, state = gen_data.create_city_state_zip()
	
	vCard = 'BEGIN:VCARD\r\n'
	vCard += 'VERSION:4.0\r\n'
	vCard += 'FN:{} {}\r\n'.format(first_name, last_name)
	vCard += 'N:{};{};;;\r\n'.format(last_name, first_name)
	vCard += 'TEL:tel:{}\r\n'.format(gen_data.create_phone())
	vCard += 'GENDER:{}\r\n'.format(gender_initial)
	vCard += 'EMAIL:{}\r\n'.format(gen_data.create_email(name=(first_name, last_name)).lower())
	vCard += 'IMPP:sip:{}@{}\r\n'.format(first_name.lower(), 'sip.linphone.org')
	vCard += 'ADR:;;{};{};{};{};\r\n'.format(adr, city, state, zip)
	vCard += 'NOTE:{}\r\n'.format(gen_data.create_sentence())
	vCard += 'ORG:{}\r\n'.format(gen_data.create_company_name())
	vCard += 'BDAY:{0:%Y%m%d}\r\n'.format(gen_data.create_birthday())
	vCard += 'END:VCARD\r\n'
	return vCard

def main():
	output = ''
	count = 1000
	widgets = [frogress.BarWidget, frogress.PercentageWidget, frogress.ProgressWidget('vCard ')]
	for i in frogress.bar(range(count), steps=count, widgets=widgets):
		output += generate_vCard()

	with codecs.open('output.vcf', 'w', 'utf-8') as f:
		f.write(output)

if __name__ == "__main__":
	main()