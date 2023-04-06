# A Python script that decrypts the content of Kamstrup KEM file and imports meter files
# to wmbusmeters' config folder.
#
# The KEM file is a (sometimes zipped) xml file that contains xml-encrypted data using 
# the xml-enc standard (http://www.w3.org/2001/04/xmlenc). The password needed to decrypt 
# the xml-encrypted data can either be the CustomerId or something else selected by the 
# person that has created the KEM file using Kamstrup software.
#
# This script takes the encrypted KEM file and decrypts its content (it automaticly detects
# the zip archive and extracts the kem file from it). The result is a XML with a list of meters 
# with their types, serial numbers, keys, etc. The script prints the information about each meter 
# to the console and populates wmbusmeters' config folder with corresponding meter files. 
# Optionally, decrypted KEM file content can be saved to a file (option -o).
#
# Usage: python kem-import.py [options] <kem_file> <password>
# kem_file ... the name of the KEM file to decrypt or a zip archive having the KEM file in it
# password ... original password used to encrypt the content (16 characters maximum)
# options  ... use -h switch to get a list of options with their descriptions
#
# The script is Python2/Python3 compatible.
#

from __future__ import print_function
import os
import sys
import re
import errno
import argparse
import base64
import zipfile
from xml.dom import minidom
from cryptography.hazmat.primitives.ciphers import Cipher, algorithms, modes
from cryptography.hazmat.backends import default_backend



help_prog = "Decrypts Kamstrup KEM file and imports meter information into wmbusmeters' config folder."
help_epilog = ""
help_kem = "The name of the KEM file to decrypt or a name of a zip archive that contains the encrypted KEM file."
help_pwd = "The original password used to encrypt the KEM file content (16 characters maximum)."
help_cfg = """Location of config files for wmbusmeters (default location is the current working directory). 
This option has the same meaning as --useconfig option of the wmbusmeters daemon, i.e. --useconfig=/ will 
populate /etc/wmbusmeters.d folder (must be run with sudo) and --useconfig=. (the default) populates 
./etc/wmbusmeters.d folder. If the destination folder does not exist, it will be created; existing meter 
files will be overwritten."""
help_dry = "No meter files will be created, only the info will be printed on the console."
help_out = "Save the decrypted KEM file content into a given file."

# define command line arguments
argparser = argparse.ArgumentParser(description=help_prog, epilog=help_epilog)
argparser.add_argument('kem_file', help=help_kem)
argparser.add_argument('password', help=help_pwd)
argparser.add_argument("-c", "--useconfig", type=str, action='store', dest='config', default=os.getcwd(), help=help_cfg)
argparser.add_argument("-n", "--dryrun", action='store_true', dest='dryrun', help=help_dry)
argparser.add_argument("-o", "--output", type=str, action='store', dest='output', help=help_out)

# parse command line arguments
args = argparser.parse_args()

# adjust the config folder location to full target path
args.config = args.config.strip(os.path.sep) + '/etc/wmbusmeters.d/'


# test if input file exists
if (not os.path.isfile(args.kem_file)):
    print('ERROR: The input KEM file does not exist.')
    sys.exit(1)
#end if

kem_file_content = None

# test if input file is a zip file
#     yes: find a file with .kem extension in the zip and extract content of that file
#     no : load the content of the input file directly
if (zipfile.is_zipfile(args.kem_file)):
    print("Detected a zip file on input ... extracting")
    with zipfile.ZipFile(args.kem_file,'r') as zipobj:
        file_list = zipobj.namelist()
        for file_name in file_list:
            if file_name.endswith('.kem') or file_name.endswith('.kem2'):
                kem_file_content = zipobj.read(file_name)
                break
            #end if
        #end for
        
        if (not kem_file_content):
            print("ERROR: The zip file '%s' does not seem to contain any '.kem' file." % (args.kem_file))
            sys.exit(1)
        #end if
else:
    # read content of the kem file
    with open(args.kem_file,'r') as f:
        kem_file_content = f.read()
#end if


# read and parse KEM file content and extract its encrypted part
# (KEM file is a a XML file formated according to http://www.w3.org/2001/04/xmlenc)
try:
    xmldoc = minidom.parseString(kem_file_content)
    encrypedtext = xmldoc.getElementsByTagName('CipherValue')[0].firstChild.nodeValue
    encrypeddata = base64.b64decode(encrypedtext)
except:
    print('ERROR: The KEM file does not seem to contain encrypted data.')
    sys.exit(1)

# KEM file data is encrypted with AES-128-CBC cipher and decryption
# requires a 128bit key and 128bit initialization vector. In the case of KEM file, 
# the initialization vector is the same as the key. If password length is less 
# than 16 characters (128 bits), we pad the key with zeros up to its full length.
key = bytes(str(args.password).encode("utf-8"))
if (len(key) < 16): key += (16-len(key)) * b"\0"

# content decryption
backend = default_backend()
cipher = Cipher(algorithms.AES(key), modes.CBC(key), backend=backend)
decryptor = cipher.decryptor()
decryptedtext = decryptor.update(encrypeddata) + decryptor.finalize()

try:
    decodedtext = decryptedtext.decode('utf-8')
except UnicodeDecodeError:
    print("ERROR: Looks like password is wrong - decryption failed!")
    sys.exit(1)

# save decrypted XML file if requested
if (args.output):
    f = open(args.output, 'wb')
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

def print_meter(meterName,meterType,meterNum,meterSerial,meterVendor,meterConfig,meterModel,meterKey):
    # meter model identification
    # CONTRIBUTING NOTE: additional meter types supported by wmbusmeters can be put here 
    # if their identification in KEM file is known
    if (meterName == 'MC302') and (meterModel.startswith('302T')): 
        wmbusmeters_driver = 'multical302'
    elif (meterName == 'MC303') and (meterModel.startswith('303')):
        wmbusmeters_driver = 'multical303'
    elif (meterName == 'MC403') and (meterModel.startswith('403')):
        wmbusmeters_driver = 'multical403'
    elif (meterName == 'MC21') and (meterModel.startswith('021')): 
        wmbusmeters_driver = 'multical21'
    elif (meterName == 'MC603') and (meterModel.startswith('603')): 
        wmbusmeters_driver = 'multical603'
    elif (meterName == 'KWM2210'): 
        wmbusmeters_driver = 'flowiq2200'
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
                f.write("driver=%s\n" % (wmbusmeters_driver))
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
                    raise
        else:
            print('    !! unknow meter type, meter file has not been created')
    #end if
#end for

# parse the decrypted KEM file content and create corresponding meter files in 
# wmbusmeters' config folder for each meter found in the XML file
trimmedText = re.sub(r'[\x00-\x08\x0b\x0c\x0e-\x1f\x7f-\xff]', '', decryptedtext.decode('utf-8'))
xmldoc = minidom.parseString(trimmedText)
if xmldoc.documentElement.tagName == "MetersInOrder":
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
        print_meter(meterName,meterType,meterNum,meterSerial,meterVendor,meterConfig,meterModel,meterKey)
elif xmldoc.documentElement.tagName == "Devices":
    for e in xmldoc.getElementsByTagName('Device'):
        # read information from source XML file 
        meterName   = e.getElementsByTagName('ShortName')[0].firstChild.nodeValue
        meterType   = e.getElementsByTagName('ConsumptionTypeName')[0].firstChild.nodeValue
        meterNum    = e.getElementsByTagName('CustomerDeviceNumber')[0].firstChild.nodeValue
        meterSerial = e.getElementsByTagName('SerialNumber')[0].firstChild.nodeValue
        meterVendor = e.getElementsByTagName('ManufacturerId')[0].firstChild.nodeValue
        meterConfig = e.getElementsByTagName('ConfigNumber')[0].firstChild.nodeValue
        meterModel  = e.getElementsByTagName('TypeNumber')[0].firstChild.nodeValue
        meterKey    = e.getElementsByTagName('Value')[0].firstChild.nodeValue
        print_meter(meterName,meterType,meterNum,meterSerial,meterVendor,meterConfig,meterModel,meterKey)
else:
    print("ERROR: Looks like password is wrong - decryption failed!")
    sys.exit(1)