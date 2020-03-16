#!/bin/bash

#input: nomi di file contenenti gli output di supervisor ($1) e client ($2)

nClients=0
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
exec 3<$1
exec 4<$2

echo "Inizio la misura..."

#leggo il file del client e ricavo gli id e i relativi secrets
done=DONE
i=0
while read -u 4 line; do
    if [[ "$line" == *"$done"* ]]; then continue
    else 
        arrayid[$i]=${line:7:16}
        secrets[$i]=${line:31}
        ((i++))
        ((nClients++))
    fi
done

#leggo il file del supervisor e creo un array con le stime dei secrets associati agli id dei clients in arrayid
i=0
while read -u 3 line; do
    LINEA=($line)
    j=0
    found=0
    while [[ $j<${nClients} && ${found}==0 ]]; do
        if [[ "$line" == *"${arrayid[$j]}"* ]]; then
            found=1
            estimates[$i]=${LINEA[2]}
        else ((j++))
        fi
    done
    ((i++))
done

#inizio le stime
for ((i=0; i<${nClients}; i++)); do
    diff=$((estimates[$i]-secrets[$i]))
    if [[ diff -lt 0 ]]; then
        diff=$((-${diff}))
    fi
    if [[ diff -le 25 ]]; then
        ((correctEst++))
    fi
    errSum=$((${errSum} + $diff))
done

avgErr=$((${errSum} / ${nClients}))

echo "Total clients and estimates: ${nClients}"
echo "Supervisor correct estimates: ${correctEst}/${nClients}"
echo "Supervisor wrong estimates: $((${nClients} - ${correctEst}))/${nClients}"
echo "Average error: ${avgErr}"
echo "DONE!"