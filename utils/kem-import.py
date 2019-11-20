# A Python script to decrypt the content of Kamstrup KEM file.
#
# The KEM file is a (sometimes zipped) xml file that contains xml-encrypted data using 
# the xml-enc standard (http://www.w3.org/2001/04/xmlenc). The password needed to decrypt 
# the xml-encrypted data can either be the CustomerId or something else selected by the 
# person that created the KEM file using Kamstrup software.
#
# This script takes the encrypted KEM file and decrypts its content. The result is also 
# a XML file with a list of meters. The script prints the information about each meter to
# the console and populates wmbusmeters' config folder with corresponding meter files. 
# Optionally, decrypted KEM file content can be saved to a file (option -o).
#
# Usage: python kem-decryptor.py [options] <kem_file> <password>
# kem_file ... the name of the KEM file to decrypt (you need to unzip the the KEM file first, if it is zipped)
# password ... original password used to encrypt the content (16 characters maximum)
# options  ... use -h switch to get a list of options with their descriptions
#
# The script is Python2/Python3 compatible.
#

from __future__ import print_function
import os
import sys
import errno
import argparse
import base64
import Crypto.Cipher.AES as AES
from xml.dom import minidom

help_prog = "Decrypts Kamstrup KEM file and imports meter information into wmbusmeters' config folder."
help_epilog = ""
help_kem = "The name of the KEM file to decrypt (you need to unzip the the KEM file first, if it is zipped)."
help_pwd = "The original password used to encrypt the KEM file content (16 characters maximum)."
help_cfg = """Location of config files for wmbusmeters (default location is the current working directory). This option has the same meaning as --useconfig option of the wmbusmeters daemon, i.e. --useconfig=/ will populate /etc/wmbusmeters.d folder (must be run with sudo) and --useconfig=. (the default) populates ./etc/wmbusmeters.d folder. If the destination folder does not exist, it will be created; existing meter files will be overwritten."""
help_dry = "No meter files will be created, only the info will be printed on the console."
help_out = "Save the decrypted KEM file content into a given file."

argparser = argparse.ArgumentParser(description=help_prog, epilog=help_epilog)
argparser.add_argument('kem_file', help=help_kem)
argparser.add_argument('password', help=help_pwd)
argparser.add_argument("-c", "--useconfig", type=str, action='store', dest='config', default=os.getcwd(), help=help_cfg)
argparser.add_argument("-n", "--dryrun", action='store_true', dest='dryrun', help=help_dry)
argparser.add_argument("-o", "--output", type=str, action='store', dest='output', help=help_out)

args = argparser.parse_args()

# adjust the config folder location to full target path
args.config = args.config.strip(os.path.sep) + '/etc/wmbusmeters.d/'


# read and parse KEM file and extract its encrypted content
# (KEM file is a a XML file formated according to http://www.w3.org/2001/04/xmlenc)
xmldoc = minidom.parse(args.kem_file)
encrypedtext = xmldoc.getElementsByTagName('CipherValue')[0].firstChild.nodeValue
encrypeddata = base64.b64decode(encrypedtext)

# KEM file data is encrypted with AES-128-CBC cipher and decryption
# requires a 128bit key and 128bit initialization vector. In the case of KEM file, 
# the initialization vector is the same as the key. If password length is less 
# than 16 characters (128 bits), we pad the key with zeros up to its full length.
key = bytes(str(args.password).encode("utf-8"))
if (len(key) < 16): key += (16-len(key)) * b"\0"

# content decryption
aes = AES.new(key, AES.MODE_CBC, IV=key)
decryptedtext = aes.decrypt(encrypeddata)

# save decrypted XML file if requested
if (args.output):
    f = open(args.output, 'w')
    f.write(decryptedtext)
    f.close()
#end if


# create the folder for meter files if it does not exists
if (not args.dryrun) and (not os.path.exists(args.config)):
    print("Creating target folder:", args.config)
    try:
        os.makedirs(args.config)
    except OSError as e:
        if (e.errno in [errno.EPERM, errno.EACCES]): 
            print(e)
            print("You may need to use 'sudo' to access the target config folder.")
            sys.exit(1)
        else:
            raise
#end if


# parse the decrypted KEM file content and create corresponding meter files in 
# wmbusmeters' config folder for each meter found in the XML file
xmldoc = minidom.parseString(decryptedtext)
for e in xmldoc.getElementsByTagName('Meter'):
    # read information from source XML file 
    meterName   = e.getElementsByTagName('MeterName')[0].firstChild.nodeValue
    meterType   = e.getElementsByTagName('ConsumptionType')[0].firstChild.nodeValue
    meterNum    = e.getElementsByTagName('MeterNo')[0].firstChild.nodeValue
    meterSerial = e.getElementsByTagName('SerialNo')[0].firstChild.nodeValue
    meterVendor = e.getElementsByTagName('VendorId')[0].firstChild.nodeValue
    meterConfig = e.getElementsByTagName('ConfigNo')[0].firstChild.nodeValue
    meterModel  = e.getElementsByTagName('TypeNo')[0].firstChild.nodeValue
    meterKey    = e.getElementsByTagName('DEK')[0].firstChild.nodeValue
    
    # meter model identification
    # CONTRIBUTING NOTE: additional meter types supported by wmbusmeters can be put here 
    # if their identification in KEM file is known
    if (meterName == 'MC302') and (meterModel.startswith('302T')): 
        wmbusmeters_driver = 'multical302'
    elif (meterName == 'MC21') and (meterModel.startswith('021')): 
        wmbusmeters_driver = 'multical21'
    else:
        wmbusmeters_driver = None
    
    # print info to console
    print('Found meter', meterName, '('+meterModel+')')
    print('    number :', meterNum)
    print('    serial :', meterSerial)
    print('    type   :', meterType)
    print('    driver :', wmbusmeters_driver)
    print('    config :', meterConfig)
    print('    key    :', meterKey)
    
    if (not args.dryrun):
        # save meter file in config folder if meter is supported by wmbusmeters
        if (wmbusmeters_driver is not None):
            try:
                f = open(args.config+meterSerial, 'w')
                f.write("name=%s\n" % (meterNum))
                f.write("type=%s\n" % (wmbusmeters_driver))
                f.write("id=%s\n" % (meterSerial))
                f.write("key=%s\n" % (meterKey))
                f.close()
                print('    meter file:', args.config+meterSerial)
            except (IOError, OSError) as e:
                if (e.errno in [errno.EPERM, errno.EACCES]): 
                    print(e)
                    print("You may need to use 'sudo' to access the target config folder.")
                    sys.exit(1)
                else:
                    print('XX',e)
                    raise
        else:
            print('    !! unknow meter type, meter file has not been created')
    #end if
#end for


