#!/bin/bash

#input: nomi di file contenenti gli output di supervisor ($1) e client ($2)

nClients=0
nEstimates=0
correctEst=0
errSum=0
avgErr=0


#controllo che i parametri di input siano file ed esistano
if ! [[ -f $1 ]]; then
    echo "ERROR: wrong input files (first)"
    exit 1
fi

if ! [[ -f $2 ]]; then
    echo "ERROR: wrong input files (second)"
    exit 1
fi

#apro i file in lettura
#exec 3<$1
#exec 4<$2

echo "Inizio la misura..."

declare -a arrayid
declare -A secrets
declare -A estimates

#leggo il file del client e ricavo gli id e i relativi secrets

while IFS= read -r line; do
    ROW=($line)
    if [[ "$line" == *"SECRET"* ]]; then
        #avevo fatto diversamente ma confrontandomi con i colleghi, mi hanno fatto notare che gli array possono essere sparsi
        #usare questa caratteristica degli array bash mi è sembrato molto conveniente e intelligente
        id=${ROW[1]}
        secrets[$id]=${ROW[3]}
        ((nClients++))
    fi
done <$2

#leggo il file del supervisor e creo un array con le stime dei secrets associati agli id dei clients in arrayid
while IFS= read -r line; do
    ROW=($line)
    if [[ "$line" == *"BASED"* ]]; then
        id=${ROW[4]}
        arrayIdEst[$nEstimates]=$id
        estimates[$id]=${ROW[2]}
        ((nEstimates++))
    fi  
done <$1

#inizio le stime
for id in "${arrayIdEst[@]}"; do
    diff=$((${estimates[$id]} - ${secrets[$id]}))
    if [[ diff -lt 0 ]]; then
        diff=$((-${diff}))
    fi
    if [[ diff -le 25 ]]; then
        ((correctEst++))
    fi
    errSum=$((${errSum} + $diff))
done

avgErr=$((${errSum} / ${nClients}))

echo "Total clients run: ${nClients}"
echo "Total estimates: ${nEstimates}"
echo "Supervisor correct estimates: ${correctEst}/${nEstimates}"
echo "Supervisor wrong estimates: $((${nClients} - ${correctEst}))/${nEstimates}"
echo "Average error: ${avgErr}"
echo "DONE!"