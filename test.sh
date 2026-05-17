#!/bin/bash

GREEN='\033[0;32m'
RED='\033[0;31m'
NC='\033[0m'

echo -e "${GREEN}=== Compilation du projet ===${NC}"
gcc -Wall -Wextra src/*.c -lcrypto -o hachement

if [ $? -ne 0 ]; then
    echo -e "${RED}Erreur de compilation !${NC}"
    exit 1
fi

TABLE_SHA="test_table.sha"
rm -f "$TABLE_SHA"

echo -e "\n${GREEN}=== Test 1: Création d'une table .sha (64 blocs) ===${NC}"
./hachement make "$TABLE_SHA" 0 64
if [ ! -f "$TABLE_SHA" ]; then
    echo -e "${RED}Échec: La table .sha n'a pas été créée.${NC}"
    exit 1
fi

echo -e "\n${GREEN}=== Test 2: Insertion de chaînes (.sha) ===${NC}"
./hachement insertstring "$TABLE_SHA" 0 "toto"
./hachement insertstring "$TABLE_SHA" 0 "titi"

echo -e "\n${GREEN}=== Test 3: Vérification de la présence (Lookup) ===${NC}"
./hachement lookup "$TABLE_SHA" 0 "toto"
if [ $? -eq 0 ]; then echo "OK: 'toto' trouvé"; else echo -e "${RED}KO: 'toto' non trouvé${NC}"; fi

echo -e "\n${GREEN}=== Test 4: Suppression par Hashcode fixe (44 pour 'toto') ===${NC}"

./hachement deletehash "$TABLE_SHA" 0 44 # 44 est le hashcode de toto
./hachement lookup "$TABLE_SHA" 0 "toto"

if [ $? -ne 0 ]; then 
    echo "OK: 'toto' supprimé"
else 
    echo -e "${RED}KO: 'toto' toujours là${NC}"
fi

echo -e "\n${GREEN}=== Fin des tests (.sha) ===${NC}"