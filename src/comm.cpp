
#include "comm.h"
#include "wx/ctb/getopt.h"
#include "wx/ctb/serport.h"
#include "wx/ctb/timer.h"
#include <stdio.h>
#include <wx/log.h>
#include "base64.h"
#include <wx/tokenzr.h>
#include <wx/hashmap.h>

CommException::CommException(int errorStatus, wxString errorDetail){
	_errorStatus = errorStatus;
	_errorDetail = errorDetail;
}

int CommException::GetErrorStatus(){
	return _errorStatus;
}

wxString CommException::GetErrorDetail(){
	return _errorDetail;
}

wxString CommException::GetErrorMessage(){

	switch (_errorStatus){
		case OPEN_PORT_FAILED:
			return "Could not open port";
		case DATA_ERROR:
			return "A data error occured";
		case COMM_TIMEOUT:
			return "A communications timeout occured: (" + _errorDetail + ")";
		default:
			return "An unspecified error occured";
	}
}

RaceAnalyzerComm::RaceAnalyzerComm() : _serialPortNumber(0), _serialPort(NULL) {

	//_logfile = fopen("mjljConfigLog.txt", "w");
	_logfile = 0;
	_logger = new wxLogStderr(_logfile);
	wxLog::SetActiveTarget(_logger);
}

RaceAnalyzerComm::~RaceAnalyzerComm(){
	CloseSerialPort();
}

void RaceAnalyzerComm::CloseSerialPort(){

	wxMutexLocker lock(_commMutex);

	wxLogMessage("closing serial port");
	if (_serialPort){
		if (_serialPort->IsOpen()) _serialPort->Close();
		delete _serialPort;
		_serialPort = NULL;
	}
}

wxSerialPort* RaceAnalyzerComm::GetSerialPort(){

	if ( _serialPort){
		if ( ! _serialPort->IsOpen()){
			delete _serialPort;
			_serialPort = NULL;
		}
	}


	if (! _serialPort){
		_serialPort = OpenSerialPort();
	}

	return _serialPort;
}

wxSerialPort* RaceAnalyzerComm::OpenSerialPort(){

	wxLogMessage("Open COM%d" ,_serialPortNumber + 1);
	wxSerialPort* comPort = new wxSerialPort();
	const char *devName = GetSerialPortDevName(_serialPortNumber);

	wxLogMessage(devName);

	wxSerialPort_DCS dcs;
	dcs.baud=wxBAUD_115200;
	dcs.stopbits=1;
	dcs.parity = wxPARITY_NONE;
	dcs.wordlen = 8;
	dcs.xonxoff = false;
	dcs.rtscts = false;

		if (comPort->Open(devName,&dcs) < 0 ){
			delete ( comPort );
			throw CommException(CommException::OPEN_PORT_FAILED);
		}
	return comPort;
}

void RaceAnalyzerComm::checkThrowResult(wxString &result){
	if (getParam(result,"result") != "ok") throw CommException(CommException::CMD_ERROR,result);
}

void RaceAnalyzerComm::unescape(wxString &data){

	wxString working = data;
	data = "";
	const char *d = working.ToAscii();
	while (*d){
		if (*d =='\\'){
			switch(*(d + 1)){
				case '_':
					data+=" ";
					break;
				case 'n':
					data+="\n";
					break;
				case 'r':
					data+="\r";
					break;
				case '\\':
					data+="\\";
					break;
				case '"':
					data+="\"";
					break;
				case '\0': //this should *NOT* happen
					return;
			}
			d++;
		}
		else{
			data+=*d;
		}
		d++;
	}
}

void RaceAnalyzerComm::escape(wxString &data){

	wxString working = data;
	data="";
	const char *d = working.ToAscii();
	while(*d){
		switch(*d){
		case ' ':
			data+="\\_";
			break;
		case '\n':
			data+="\\n";
			break;
		case '\r':
			data+="\\r";
			break;
		case '\"':
			data+="\\\"";
			break;
		default:
			data+=*d;
			break;
		}
		d++;
	}
}

void RaceAnalyzerComm::reloadScript(void){

	wxSerialPort *serialPort = GetSerialPort();
	if (NULL==serialPort) throw CommException(CommException::OPEN_PORT_FAILED);

	wxString reloadCmd = "reloadScript";
	wxString result = SendCommand(serialPort, reloadCmd);
	checkThrowResult(result);
}

wxString RaceAnalyzerComm::readScript(){

	wxString script = "";
	int scriptPage = 0;
	int to = 0;
	wxLogMessage("readScript");
	wxSerialPort *serialPort = GetSerialPort();
	FlushReceiveBuffer(serialPort);
	while(!to){

		//wxString cmd = wxString::Format("println(getScriptPage(%d))",scriptPage++);
		wxString cmd = wxString::Format("readScriptPage %d",scriptPage++);
		wxString buffer = SendCommand(serialPort, cmd);
		wxLogMessage("read: %s",buffer.ToAscii());
		buffer.Trim(false);
		buffer.Trim(true);

		size_t scriptFragmentLen = buffer.Length();
		wxString scriptFrag = getParam(buffer,"script");
		unescape(scriptFrag);

		wxLogMessage("unescaped script'%s",scriptFrag.ToAscii());

		if (scriptFragmentLen > 0 ) script+=scriptFrag;
		//the last page is a 'partial page'
		if (scriptFragmentLen < SCRIPT_PAGE_LENGTH ) break;
	}
	CloseSerialPort();
	if (to){
		throw CommException(CommException::COMM_TIMEOUT);
	}
	return script;
}

void RaceAnalyzerComm::writeScript(wxString &script){

	size_t index = 0;
	int page,to;
	page = 0;
	to = 0;
	size_t length = script.Length();

	wxSerialPort *serialPort = GetSerialPort();

	while(index < length && page < SCRIPT_PAGES && !to){
		wxString scriptFragment;
		if (index + SCRIPT_PAGE_LENGTH > length){
			scriptFragment = script.Mid(index);
		}else{
			scriptFragment = script.Mid(index, SCRIPT_PAGE_LENGTH);
		}
		wxLogMessage("script fragment: '%s'", scriptFragment.ToAscii());
		escape(scriptFragment);
		wxLogMessage("esc script fragment: '%s'",scriptFragment.ToAscii());
		//wxString cmd = wxString::Format("updateScriptPage(%d,\"%s\")", page,data.ToAscii());
		wxString cmd = wxString::Format("writeScriptPage %d %s",page,scriptFragment.ToAscii());
		wxString result = SendCommand(serialPort, cmd);
		checkThrowResult(result);
		page++;
		index += SCRIPT_PAGE_LENGTH;
	}
	//did we write fewer than the max number of script pages?
	//note we're subtracting script pages by one to account for integer divide truncation
	if ((length / SCRIPT_PAGE_LENGTH) < SCRIPT_PAGES - 1 ){
		//write a null to the next page
		wxString cmd = wxString::Format("writeScriptPage %d",page);
		SendCommand(serialPort, cmd);
	}

	CloseSerialPort();
	if (to){
		throw CommException(CommException::COMM_TIMEOUT);
	}
}

void RaceAnalyzerComm::SetSerialPort(int port){

	wxMutexLocker lock(_commMutex);

	if (_serialPortNumber != port){

		if (_serialPort){
			if (_serialPort->IsOpen()) _serialPort->Close();
			delete _serialPort;
			_serialPort = NULL;
		}
		_serialPortNumber = port;
	}
}

const char * RaceAnalyzerComm::GetSerialPortDevName(int comPort){

	switch (comPort){
		case 0:
			return wxCOM1;
		case 1:
			return wxCOM2;
		case 2:
			return wxCOM3;
		case 3:
			return wxCOM4;
		case 4:
			return wxCOM5;
		case 5:
			return wxCOM6;
		case 6:
			return wxCOM7;
		case 7:
			return wxCOM8;
		case 8:
			return wxCOM9;
		case 9:
			return wxCOM10;
		case 10:
			return wxCOM11;
		case 11:
			return wxCOM12;
		case 12:
			return wxCOM13;
		case 13:
			return wxCOM14;
		case 14:
			return wxCOM15;
		case 15:
			return wxCOM16;
		case 16:
			return wxCOM17;
		case 17:
			return wxCOM18;
		case 18:
			return wxCOM19;
		case 19:
			return wxCOM20;
		default:
			throw CommException(CommException::OPEN_PORT_FAILED);
	}
}

int RaceAnalyzerComm::FlushReceiveBuffer(wxSerialPort* comPort){

	char data;
	int buffer = 0;
	if (comPort && comPort->IsOpen()){
		while (1){
			if (comPort->Read(&data,1)){
				//#ifdef __WXDEBUG__
				wxLogMessage("flushed %0x", (int)data);
				//#endif
				//wxBell();
				buffer++;
			}
			else{
				break;
			}
		}
	if (buffer > 0) wxLogMessage("flushed %d bytes",buffer);
	}
	return buffer;
}

wxString RaceAnalyzerComm::SendCommand(wxSerialPort *comPort, wxString &buffer, int timeout){
	FlushReceiveBuffer(comPort);
	wxLogMessage("writing %d: %s",buffer.Len(), buffer.ToAscii());
	WriteLine(comPort,buffer,timeout);

	{
		wxString echo;
		int to = ReadLine(comPort,echo,timeout);
		if (to) throw CommException(CommException::COMM_TIMEOUT);
		if (echo != buffer){
			wxLogMessage("echoed buffer was %s instead of %s", echo.ToAscii(), buffer.ToAscii());
			//we expected the command echoed back
			//throw CommException(CommException::DATA_ERROR);
		}
	}
	{
		wxString result;
		int to = ReadLine(comPort,result,timeout);
		if (to) throw CommException(CommException::COMM_TIMEOUT);
		result.Trim(false);
		result.Trim(true);
		return result;
	}
}

int RaceAnalyzerComm::WriteLine(wxSerialPort * comPort, wxString &buffer, int timeout){

	wxLogMessage("writeLine: %s", buffer.ToAscii());
	char *tempBuff = (char*)malloc(buffer.Len() + 10);
	strcpy(tempBuff,buffer.ToAscii());
	strcat(tempBuff,"\r");
	char *buffPtr = tempBuff;

	int to = 0;
	timer t(timeout, &to, NULL);
	t.start();

	size_t len = strlen(buffPtr);
	while (len > 0 && !to){
		size_t written = comPort->Writev(buffPtr,len,&to,true);
		buffPtr+=written;
		len -= written;
	}
	t.stop();
	free(tempBuff);

	return to;
}

int RaceAnalyzerComm::ReadLine(wxSerialPort * comPort, wxString &buffer, int timeout){

	int to = 0;
	timer t(timeout, &to, NULL);

	t.start();

	wxString dataLine = "";
	char value;

	while(!to){
		if (comPort->Readv(&value,1,&to,true)){
			if ('\n' == value) break;
			buffer += value;
		}
	}
	t.stop();

	if (to){
		wxLogMessage("timed out");
		//CloseSerialPort();
	}

//	wxLogMessage("readLine: ");
//	wxLogMessage(buffer.T);
	return to;
}

void RaceAnalyzerComm::swapCharsInsideQuotes(wxString &data, char find,char replace){
	bool insideQuotes = false;
	int len = data.Len();
	for (int index = 0; index < len; index++){
		if (data[index] == '"') insideQuotes = !insideQuotes;
		else if (data[index] == find && insideQuotes) data[index] = replace;
	}
}

void RaceAnalyzerComm::hideInnerTokens(wxString &data){
	swapCharsInsideQuotes(data,'=',EQUALS_HIDE_CHAR);
	swapCharsInsideQuotes(data,';',SEMICOLON_HIDE_CHAR);
}

void RaceAnalyzerComm::unhideInnerTokens(wxString &data){
	swapCharsInsideQuotes(data,EQUALS_HIDE_CHAR,'=');
	swapCharsInsideQuotes(data,SEMICOLON_HIDE_CHAR,';');
}

wxString RaceAnalyzerComm::getParam(wxString &data, const wxString &name){

	hideInnerTokens(data);
	wxStringTokenizer tokenizer(data,";");

	while (tokenizer.HasMoreTokens()){
		wxString token = tokenizer.GetNextToken();
		wxStringTokenizer subTokenizer(token,"=");
		if (subTokenizer.CountTokens() >= 2){
			wxString paramName = subTokenizer.GetNextToken();
			if (paramName == name){
				wxString value = subTokenizer.GetNextToken();
				unhideInnerTokens(value);
				if (value.StartsWith("\"") && value.EndsWith("\"")){
					value = value.Left(value.Len() - 1);
					value = value.Right(value.Len() - 1);
				}
				return value;
			}
		}
	}
	return "";
}
