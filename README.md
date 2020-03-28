# OOB-Signaling
Out-Of-Band Signaling. Operating Systems exam project @ University of Pisa


Il progetto ha lo scopo di implementare un sistema per l’out-of-band signaling. Si tratta di
una tecnica di comunicazione in cui due entità si scambiano informazioni senza trasmettersele
direttamente, ma utilizzando segnalazione collaterale. Lo scopo è quello di rendere difficile
l’intercettazione di queste informazioni “private” da parte di un terzo utente che potrebbe star
catturando i dati in transito.
In questo specifico progetto, è stato realizzato un sistema client-server in cui i client possie-
dono un codice segreto (secret) che vogliono comunicare a un server centrale, evitando la sua
trasmissione diretta. La tecnica di segnalazione collaterale implementata si basa sull’utilizzo
di timer (nei client) e su misurazioni dei tempi di ricezione di messaggi consecutivi (nei server).
Un client, infatti, comunica con un server inviando un messaggio. Aspetta poi secret mil-
lisecondi prima di inviare il successivo. Un server misura il tempo che passa tra la ricezione
di un messaggio del client e la ricezione del successivo, cosı̀ da stimare il valore del secret.
Le stime raccolte verranno poi comunicate a un server centrale, chiamato supervisor.
Si avranno n client, k server e un supervisor.
