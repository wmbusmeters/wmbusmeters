
#Black        0;30     Dark Gray     1;30
#Red          0;31     Light Red     1;31
#Green        0;32     Light Green   1;32
#Brown/Orange 0;33     Yellow        1;33
#Blue         0;34     Light Blue    1;34
#Purple       0;35     Light Purple  1;35
#Cyan         0;36     Light Cyan    1;36
#Light Gray   0;37     White         1;37

GREEN='\033[37;1;42m'
#RED='\033[31;1;41m'
RED='\033[1;37;41m'
NC='\033[0m'

printERROR() {
   local script_name
   script_name="$(basename "$0")"
   printf "${RED} ERROR ${NC} [%s] $@\n" "$script_name"
}

printOK() {
    printf "${GREEN} OK ${NC} $@\n"
}
