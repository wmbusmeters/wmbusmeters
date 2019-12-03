
## Utilities

This folder contains a set of utilities that may become usefull when working with wmbusmeters.

### Analyze.java

A utility to analyze telegrams.

### XMLExtract.java
Deciphers and prints out the content of encrypted XML file. The encrypted file must use W3C XML Encryption Recommendation with [128bit AES-CBC cipher](http://www.w3.org/2001/04/xmlenc#aes128-cbc).

**Usage:**

    java -cp . XMLExtract [password] [encrypted_xml_file]

### kem-import.py

Extracts meter information from Kamstrup KEM file and imports meter files to wmbusmeters' config folder. 

The KEM file is a (sometimes zipped) xml file that contains xml-encrypted data using the [xml-enc standard](http://www.w3.org/2001/04/xmlenc). The password needed to decrypt the xml-encrypted data can either be the CustomerId or something else selected by the person that has created the KEM file using Kamstrup software.

The script takes the encrypted KEM file and decrypts its content (it automaticly detects if the file is a zip archive and extracts the kem file from it). The result is a XML with a list of meters with their types, serial numbers, keys, etc. The script prints the information about each meter to the console and populates wmbusmeters' config folder with corresponding meter files.
Optionally, decrypted KEM file content can be saved to a file.

**Usage:**

    python kem-import.py [options] <kem_file> <password>

Use -h switch to get the full list of options or see the source file for more info. If you want to import to system's config folder (`-c /etc` switch), you neet to use sudo (`sudo python kem-import.py ...`) to get write access to it.



