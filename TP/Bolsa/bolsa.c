#include "utils.h"


#include <windows.h>
#include <tchar.h>
#include <stdio.h>
#include <io.h>
#include <fcntl.h>

HANDLE clientes[MAXCLIENTES];

void iniciaClientes() {}

void adicionaCliente(HANDLE cli) {}

void removeCliente(HANDLE cli) {}

HANDLE WriteReady;

int writeClienteASINC(HANDLE hpipe, MSG msg) {
	DWORD cbWritten = 0;
	BOOL fSuccess = FALSE;

	OVERLAPPED OverlWr = { 0 };

	_tprintf(TEXT("\nA enviar %d bytes com %s\n"), (int)Msg_Sz, msg.msg);

	ZeroMemory(&OverlWr, sizeof(OverlWr)); //nao necessario pq { 0 }
	ResetEvent(WriteReady); //nao assinalado
	OverlWr.hEvent = WriteReady;

	fSuccess = WriteFile(
		hPipe, //handle para o pipe
		&msg, //message (ponteiro)
		Msg_Sz, //comprimento da mensagem
		&cbWritten, //ptr para guardar numero de bytes escritos
		&OverlWr //!= NULL -> mesmo overlapped I/O
	);

	WaitForSingleObject(WriteReady, INFINITE); // <- colocar aqui um valor
	GetOverlappedResult(hpipe, &OverlWr, &cbWritten, FALSE); //sem WAIT
	if (cbWritten == Msg_sz) {
		_tprintf(TEXT("\nWrite para 1 cliente concluido"));
		return 1;
	}
	else {
		PrintLastError(TEXT("\nOcorreu algo na escrita para 1 cliente"));
		return 0;
	}
}

int broadcastClientes(MSG msg) {
	int i, numerites = 0;
	for (int i = 0; i < MAXCLIENTES; i++) {
		if (clientes[i] != NULL) {
			if (writeClienteASINC(clientes[i], msg))
				++numerites;
		}
		else {
			//aconteceu alguma coisa com este cliente -> tirar o tipo da lista (?)
			removeCliente(clientes[i]); //ou apenas clientes[i] = NULL;
			_tprintf(TEXT("\nCliente suspeito removido."));
		}
	}
	return numerites;
}

int _tmain(VOID) {
	BOOL fConnected = FALSE;
	DWORD dwThreadId = 0;
	HANDLE hPipe = INVALID_HANDLE_VALUE, hThread = NULL;
	LPTSTR lpsrPipename = TEXT("\\\\.\\pipe\\???"); //nao apanhei esta

	_setmode(_fileno(stdout), _O_WTEXT);
	_setmode(_fileno(stdin), _O_WTEXT);
	_setmode(_fileno(stderr), _O_WTEXT);

	WriteReady = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (WriteReady == NULL) {
		_tprintf(TEXT("\nNao foi possivel criar o evento WriteReady"));
		return 1;
	}

	iniciaClientes();

	while (1) {
		_tprintf(TEXT("\nServidor - ciclo principal - creatednamedpipe - %s"), lpsrPipename);

		hPipe = CreateNamedPipe(
			lpsrPipename, //nome de pipe
			PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED, //acesso
			PIPE_TYPE_MESSAGE, // tipo de pipe - message
			PIPE_WAIT, //? ? message-read
			PIPE_UNLIMITED_INSTANCES, //bloqueante
			BUFSIZ,//tem buffer ?
			BUFSIZ,//tem buffer input
			5000,//time out p/cliente em milissegundos
			NULL //atributos segunraça default
		);

		if (hPipe == INVALID_HANDLE_VALUE) {
			_tprintf(TEXT("\ncREATEnamepipe falhou erro %d"));
			return -1;
		}
	}

	_tprintf(TEXT("\nServidor a aguardar que um cliente se liga"));
	//aguarda que um cliente se ligue
	// != 0 significa sucesso
	// = 0 = lastError = error_pipe_connect() significa sucesso tmb

	fConnected = ConnectNamedPipe(hPipe, NULL) ? TRUE : (GetLastError() == ERROR_PIPE_CONNECTED);

		if (fConnected) { //se há um cliente ligado, atendo
			_tprintf(TEXT("\nCliente ligado. Vou criar uma thread para ele"));

			hThread = CreateThread(
				NULL, //sem atributos de segurança
				0, //tam pilha default
				InstanceThread, //funcao da thread
				(LPVOID)hPipe, //parametro para a thread = handle || aqui vai para a estrutura de dados
				0,  //incialmente nao suspensa
				&dwThreadId //ptr p/ onde colocar o ID da thread, pode ser um array
			);

			if (hThread == NULL) {
				_tprintf(TEXT("\nErro na criação da thread : Erro = %d"), GetLastError());
				return -1;
			}
			else {
				closeHandle(hThread); //nao precisa de handle p/ a thread
			}
		}
		else {
			closeHandle(hThread); //o cliente nao conseguiu ligar, fecha esta instancia do pipe
		}

	//FALTA ENCERRAR

	return 0; //encerra servidor
}

//função da thread
//atende um cliente pode estar em ciclo, a trocar mensagens
//com o cliente (varios read/write) entrentanto, o ciclo
//principal do servidor ja avancou de forma independente
//eventualmente atendendo novos clientes com novas instancias
//o parametro é o handle para o pipe

//a assinalar apenas isto: a rececao de uma mensagem num cliente
//vai causar o envio da mensagem a todos os clientes
//seguindo a logica do broadcast
//-> os outros clientes vao ter que lidar com uma mensagem do servidor
//que nao estavam especificamente a contar receber nessa altura

DWORD WINAPI InstanceThread(LPVOID lpvParam) {
	Msg Pedido, Resposta;
	DWORD cbBytesRead = 0, cbReplyBytes = 0, cbWriteXXXXtem = 0;
	int numresp = 0;
	BOOL fSucess = FALSE;
	HANDLE hPipe = (HANDLE)lpvParam; //a informação enviada é o handle

	HANDLE ReadReady;
	OVERLAPPED OverlRd = { 0 }; //ou OVERLAPPED OverlRd = ( 0 ); nao consegui perceber

	if (hPipe == NULL) {
		_tprintf(TEXT("\nErro - o handle enviado no param da thread é nulo"));
		return -1;
	}

	ReadReady = CreateEvent(
		NULL, //sem atributos de segurança + non inheritable
		TRUE, //reset manual, por requisito do overlapped
		FALSE, //estado inicial = not sigmaled
		NULL //nao precisa de nome: uso interno ao processo
	);

	if (ReadReady == NULL) {
		_tprintf(TEXT("\nservidor nao foi possivel criar o evento read"));
		return -1;
	}

	adicionaCliente(hPipe);
	_tprintf(TEXT("\nThread dedicada ao servidor - a receber mensagens"));

	//ciclo de dialogo com o cliente
	while (1) { //termina mediante uma condição qualquer

		//Obtem mensagens do cliente
		ZeroMemory(&OverlRd, sizeof(OverlRd));
		ResetEvent(ReadReady);
		OverlRd.hEvent = ReadReady;
		fSuccess = ReadFile(
			hPipe, //handle para o pipe recebido no param
			&Pedido,//buffer para os dados a ler
			Msg_Sz, //tamanho msg a ler
			&cbBytesRead,//numeros de bytes lidos
			&OverlRd //!= NULL -> é overlapped IO
		);
		_tprintf(TEXT("\nA espera da conclusao do Read"));
		WaitForSingleObject(ReadReady, INFINITE);
		_tprintf(TEXT("\nRead do cliente concluido"));

		//testar o cliente, pode ter desligado
		GetOverlappedResult(hPipe, &OverlRd, &cbBytesRead, FALSE);
		if (cbBytesRead < Msg_Sz) {
			PrintLastError(TEXT("\nRead file falhou"), GetLastError());
			break;
		}

		//processa a mensagem recebida
		_tprintf(TEXT("\nServido: recebi msg [%s]"), Pedido.msg);
		_tcscpy_s(Resposta.msg[i] = _toupper(Resposta.msg[i]));

		//Escreve a resposta no pipe para todos
		numresp = broadcastClientes(Resposta);
		_tprintf(TEXT("\nServidor respostas enviadas"), numresp);
	} //termina quando dá erro na leitura do pipe

	//vai desligar a ligação ao cliente
	//antes deve garantir que todos os dados da resposta foram mesmo escritos com FlushFileBuffers (?)

	removeCliente(hPipe);

	FlushFileBuffers(hPipe);
	DisconnectNamedPipe(hPipe); //desliga o servidor da instancia
	CloseHandle(hPipe); //fecha este lado da instancia

	_tprintf(TEXT("\nThread dedicada. Cliente a terminar"), numresp);
}
