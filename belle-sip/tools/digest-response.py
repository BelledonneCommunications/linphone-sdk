#!/usr/bin/env python

############################################################################
# digest-response.py
# Copyright (C) 2015  Belledonne Communications, Grenoble France
#
############################################################################
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
#
############################################################################

import hashlib
import sys

import argparse
def int16(x):
	return int(x, 16)
	
def main(argv=None):
	parser = argparse.ArgumentParser(description='Compute response parameter for a digest authetication. ex %(prog)s --qop-auth --cnonce f1b8598a --nonce-count 2b --userid toto --realm sip.linphone.org --password secret REGISTER sip:sip.linphone.org QPUC2gAAAABufvQmAABDq5AKuv4AAAAA')
	parser.add_argument('--userid',help='User identifier')
	parser.add_argument('--realm',help='realm as defined by the server in 401/407')
	parser.add_argument('--password',help='clear text password')
	parser.add_argument('method',help='sip method of the challanged request line')
	parser.add_argument('uri',help='sip uri of the challanged request uri')
	parser.add_argument('nonce',help='Nonce param as defined by the server in 401/407')
	
	parser.add_argument('--ha1',help='ha1 MD5(username:realm:password)')
	parser.add_argument('--qop-auth', help='Indicate if auth mode has to reuse nonce (I.E qop=auth',action='store_true')
        
	parser.add_argument('--cnonce', help='client nonce')
	parser.add_argument('--nonce-count',type=int16, help='nonce count in hexa: ex 2b')
	parser.add_argument('--algorithm', help='choose algorithm from MD5 and SHA256')
					
	
	args = parser.parse_args(argv)

	if not args.ha1 :
        	if not args.algorithm:
            		#HA1=MD5(username:realm:password)
            		ha1 = hashlib.md5()
		else:
			#HA1=SHA256(username:realm:password)
			ha1 = hashlib.sha256()
		ha1.update((args.userid+":"+args.realm+":"+args.password).encode())
		ha1_value = ha1.hexdigest()
	else:
		ha1_value = args.ha1

	if not args.algorithm:
		#HA2=MD5(method:digestURI)
		ha2 = hashlib.md5()
	else:
		#HA2=SHA256(method:digestURI)
		ha2 = hashlib.sha256()
	
	ha2.update((args.method+":"+args.uri).encode())
	print ("ha1 = "+ha1_value);
	print ("ha2 = "+ha2.hexdigest());		
	
	if args.qop_auth :
		if not args.cnonce and not args.nonce_count :
			print ("--qop-auth requires both --cnonce and --nonce-count")
			sys.exit(-1)
		
		if not args.algorithm:
			#response=MD5(HA1:nonce:nonceCount:clientNonce:qop:HA2)
			response = hashlib.md5()
		else:
			#response=SHA256(HA1:nonce:nonceCount:clientNonce:qop:HA2)
			response = hashlib.sha256()
        
		response.update(		(ha1_value
							+":"+args.nonce
							+":" + '{:08x}'.format(args.nonce_count)
							+":" + args.cnonce
							+":auth"
							+":" + ha2.hexdigest()).encode())
		print ("responce = "+response.hexdigest());	
				
	
	else:
		if not args.algorithm:
			#response=MD5(HA1:nonce:HA2)
			response = hashlib.md5()
		else:
			#response=SHA256(HA1:nonce:HA2)
			response = hashlib.sha256()
		
		response.update((ha1_value+":"+args.nonce+":"+ha2.hexdigest()).encode())
		print ("responce = "+response.hexdigest());	
			
		



if __name__ == "__main__":
	sys.exit(main())

	
	
