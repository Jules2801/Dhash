#!/bin/bash

GREEN='\033[0;32m'
RED='\033[0;31m'
NC='\033[0m'

echo -e "${GREEN}=== Compilation du projet ===${NC}"
make

if [ $? -ne 0 ]; then
    echo -e "${RED}Erreur de compilation !${NC}"
    exit 1
fi

TABLE_SHA="test_string.sha"
TABLE_FILE="test_file.sha"
SAMPLE_FILE="dictionnaire_officiel_scrabble_2012.txt"

rm -f "$TABLE_SHA"
rm -f "$TABLE_FILE"

echo -e "\n${GREEN}=== Test 1: Création d'une table .sha (64 blocs) ===${NC}"
./hachement make "$TABLE_SHA" 0 64
if [ ! -f "$TABLE_SHA" ]; then
    echo -e "${RED}échec: La table .sha n'a pas été créée.${NC}"
    exit 1
fi

echo -e "\n${GREEN}=== Test 2: Insertion de chaînes (.sha) ===${NC}"
./hachement insertstring "$TABLE_SHA" 0 "toto"
./hachement insertstring "$TABLE_SHA" 0 "titi"

echo -e "\n${GREEN}=== Test 3: Vérification de la présence (Lookup) ===${NC}"
./hachement lookup "$TABLE_SHA" 0 "toto"
if [ $? -eq 0 ]; then echo "OK: 'toto' trouvé"; else echo -e "${RED}KO: 'toto' non trouvé${NC}"; fi

echo -e "\n${GREEN}=== Test 4: Suppression par Hashcode fixe  ===${NC}"

./hachement deletehash "$TABLE_SHA" 0 "3" # 3 est le hashcode de toto
./hachement lookup "$TABLE_SHA" 0 "toto"

if [ $? -ne 0 ]; then 
    echo "OK: 'toto' supprimé"
else 
    echo -e "${RED}KO: 'toto' toujours là${NC}"
fi

echo -e "\n${GREEN}=== Test 5: Insertion de fichier (.sha) ===${NC}"
./hachement make "$TABLE_FILE" 0 6400

free_before=$(./hachement free "$TABLE_FILE" 0 | grep 'Blocs libres' | awk '{print $4}')
./hachement insertfile "$TABLE_FILE" 0 "$SAMPLE_FILE"
free_after=$(./hachement free "$TABLE_FILE" 0 | grep 'Blocs libres' | awk '{print $4}')

echo "Blocs libres avant insertion fichier: $free_before"
echo "Blocs libres après insertion fichier:  $free_after"

if [ -n "$free_before" ] && [ -n "$free_after" ] && [ "$free_after" -lt "$free_before" ]; then
    echo "OK: le fichier a bien été inséré"
else
    echo -e "${RED}KO: l'insertion du fichier n'a pas réduit le nombre de blocs libres${NC}"
fi

echo -e "\n${GREEN}=== Test 6: Lookup du fichier inséré ===${NC}"
echo "info" | ./hachement lookup "$TABLE_FILE" 0 "$SAMPLE_FILE"
if [ $? -ne 0 ]; then
    echo -e "${RED}KO: le fichier n'a pas été retrouvé par lookup${NC}"
else
    echo "OK: le fichier a été retrouvé par lookup"
fi

echo -e "\n${GREEN}=== Test 6b: Print du fichier inséré ===${NC}"
printf "print\n\n" | ./hachement lookup "$TABLE_FILE" 0 "$SAMPLE_FILE" > test_print.txt
if diff "$SAMPLE_FILE" test_print.txt > /dev/null; then
    echo "OK: le contenu du fichier correspond (print)"
else
    echo -e "${RED}KO: le contenu du fichier ne correspond pas (print)${NC}"
fi
rm -f test_print.txt

echo -e "\n${GREEN}=== Test 6c: Save du fichier inséré ===${NC}"
printf "save\n\ntest_save.txt\n" | ./hachement lookup "$TABLE_FILE" 0 "$SAMPLE_FILE"
if diff "$SAMPLE_FILE" test_save.txt > /dev/null; then
    echo "OK: le contenu du fichier correspond (save)"
else
    echo -e "${RED}KO: le contenu du fichier ne correspond pas (save)${NC}"
fi
rm test_save.txt

echo -e "\n${GREEN}=== Test 7: Suppression de chaîne avec delete ===${NC}"
./hachement insertstring "$TABLE_SHA" 0 "tata"
./hachement delete "$TABLE_SHA" 0 "tata"
./hachement lookup "$TABLE_SHA" 0 "tata"
if [ $? -ne 0 ]; then
    echo "OK: 'tata' supprimé par delete"
else
    echo -e "${RED}KO: 'tata' toujours là après delete${NC}"
fi

echo -e "\n${GREEN}=== Test 8: Suppression de fichier avec delete ===${NC}"
./hachement delete "$TABLE_FILE" 0 "$SAMPLE_FILE"
echo "info" | ./hachement lookup "$TABLE_FILE" 0 "$SAMPLE_FILE"
if [ $? -ne 0 ]; then
    echo "OK: le fichier a bien été supprimé par delete"
else
    echo -e "${RED}KO: le fichier est toujours là après delete${NC}"
fi


echo -e "\n${GREEN}=== Test 9: Insertions concurrentes en multiprocess ===${NC}"
TABLE_CONC="test_concurrent.sha"
rm -f "$TABLE_CONC"
./hachement make "$TABLE_CONC" 5 1024

for i in {1..20}; do
    ./hachement insertstring "$TABLE_CONC" 5 "concurrent_$i" >/dev/null &
done
wait

success=0
for i in {1..20}; do
    ./hachement lookup "$TABLE_CONC" 0 "concurrent_$i" >/dev/null 2>&1
    if [ $? -eq 0 ]; then
        success=$((success + 1))
    fi
done

if [ "$success" -eq 20 ]; then
    echo "OK: Les 20 insertions concurrentes ont réussi."
else
    echo -e "${RED}KO: Seulement $success/20 insertions ont réussi.${NC}"
fi
rm -f "$TABLE_CONC"

echo -e "\n${GREEN}=== Fin des tests (.sha) ===${NC}"
