unit Main;

{$mode objfpc}{$H+}

interface

uses
  Classes, SysUtils, Forms, Controls, Graphics, Dialogs, StdCtrls, Grids, ExtCtrls, Menus, PairSplitter,
  USB_HID_ReadWrite, ArrayOfTypes, RingBufHelper, EditNum;

type
  TCANData=packed record
    id:Word;
    data:TValue8b;
  end;
  PCANData=^TCANData;


  { TForm1 }

  TForm1 = class(TForm)
    btnClear: TButton;
    btnClearFiltrCANIDList: TButton;
    btnSendRow: TButton;
    btnOpenDev: TButton;
    btnCloseDev: TButton;
    btnGetDevList: TButton;
    cbDeviceList: TComboBox;
    cbFiltrCANIDList: TComboBox;
    cbStopList: TCheckBox;
    enBufLen: TEditNum;
    lb4CANMsg: TListBox;
    MainMenu1: TMainMenu;
    miSaveRecv: TMenuItem;
    miClose: TMenuItem;
    miSaveSends: TMenuItem;
    miLoadSends: TMenuItem;
    MenuItem3: TMenuItem;
    mmiFile: TMenuItem;
    opdCommon: TOpenDialog;
    PairSplitter1: TPairSplitter;
    PairSplitterSide1: TPairSplitterSide;
    PairSplitterSide2: TPairSplitterSide;
    s4ShowConnectStatus: TShape;
    StringGrid1: TStringGrid;
    svdCommon: TSaveDialog;
    tm4UpdateMsgList: TTimer;
    procedure btnClearClick(Sender: TObject);
    procedure btnClearFiltrCANIDListClick(Sender: TObject);
    procedure btnCloseDevClick(Sender: TObject);
    procedure btnGetDevListClick(Sender: TObject);
    procedure btnOpenDevClick(Sender: TObject);
    procedure btnSendRowClick(Sender: TObject);
    procedure cbFiltrCANIDListKeyDown(Sender: TObject; var Key: Word; Shift: TShiftState);
    procedure cbFiltrCANIDListKeyPress(Sender: TObject; var Key: char);
    procedure enBufLenKeyPress(Sender: TObject; var Key: char);
    procedure FormClose(Sender: TObject; var CloseAction: TCloseAction);
    procedure FormCreate(Sender: TObject);
    procedure miCloseClick(Sender: TObject);
    procedure miLoadSendsClick(Sender: TObject);
    procedure miSaveRecvClick(Sender: TObject);
    procedure miSaveSendsClick(Sender: TObject);
    procedure StringGrid1SelectCell(Sender: TObject; aCol, aRow: Integer; var CanSelect: Boolean);
    procedure tm4UpdateMsgListTimer(Sender: TObject);
  private
    FUSBWorker:TUSBHIDReadWrite;
    FCANMassege:TArrayOfString;
    FFiltrList:TArrayOfInteger;
    FStrRingBuff:TRingBufHelper;
    FConnectBreak:Boolean;
    FSendRow:Integer;
    FRecCount:Integer;
    FListBufLen:Integer;

    procedure HIDDataIn(buf:PByte; cnt:Integer);
    procedure NoConnectWithDev(Sender:TObject);
    procedure LoadSendsMessages(fname:string);
    procedure SaveSendsMessages(fname:string);
    procedure SaveRevsMessages(fname:string);
  end;

var
  Form1: TForm1;

implementation

{$R *.lfm}

{ TForm1 }

procedure TForm1.FormCreate(Sender: TObject);
var
  i:Integer;
begin
  for i:=1 to StringGrid1.RowCount-1 do StringGrid1.Cells[0, i]:=i.ToString;
  FSendRow:=1; // Номер строки из которой берутся данные для отправки в USB, а затем по CAN
  FListBufLen:=enBufLen.TxtToInt;

  FUSBWorker:=TUSBHIDReadWrite.Create;
  FUSBWorker.OnDataIn:=@HIDDataIn;
  FUSBWorker.OnNoConnect:=@NoConnectWithDev;

  SetLength(FCANMassege, 50);
  FStrRingBuff:=TRingBufHelper.Create(Length(FCANMassege));

  btnGetDevListClick(nil);  // Чтение списка устройств при запуске

  if FileExists('DefaultsSendsCANMsg.txt') then    // Если найден файл по умолчанию, то
    LoadSendsMessages('DefaultsSendsCANMsg.txt');  // загружаем данные из него
end;

procedure TForm1.miCloseClick(Sender: TObject);
begin
  Close;
end;

procedure TForm1.miLoadSendsClick(Sender: TObject); // Загрузить отправляемые сообщения

begin
  if not opdCommon.Execute then Exit;
  LoadSendsMessages(opdCommon.FileName);  // Загрузка из файла отправляемых сообщений
end;

procedure TForm1.miSaveRecvClick(Sender: TObject); // Сохранение того, что принято на текущий момент
begin
  if not svdCommon.Execute then Exit;
  SaveRevsMessages(svdCommon.FileName);
end;

procedure TForm1.miSaveSendsClick(Sender: TObject); // Сохранение списка отправляемых сообщений
begin
  if not svdCommon.Execute then Exit;
  SaveSendsMessages(svdCommon.FileName);
end;

procedure TForm1.StringGrid1SelectCell(Sender: TObject; aCol, aRow: Integer; var CanSelect: Boolean);
var
  i:Integer;
begin
  for i:=1 to StringGrid1.RowCount-1 do begin
    if i<>aRow then
      StringGrid1.Cells[0, i]:=i.ToString
    else begin
      StringGrid1.Cells[0, i]:=i.ToString + ' *';
      FSendRow:=aRow;
    end;
  end;
end;

procedure TForm1.tm4UpdateMsgListTimer(Sender: TObject);
begin
  if FConnectBreak then begin
    tm4UpdateMsgList.Enabled:=False; // Выключаем таймер
    s4ShowConnectStatus.Brush.Color:=clRed;
    ShowMessage('Разрыв соединения с USB устройством');
    Exit;
  end;

  lb4CANMsg.Items.BeginUpdate;
  repeat
    if not FStrRingBuff.CanRead then Break;
    if not cbStopList.Checked then
      lb4CANMsg.Items.Add(Format('%.6d   ',[FRecCount])+FCANMassege[FStrRingBuff.Index4Read]);
    Inc(FRecCount);
    FStrRingBuff.EndRead;
  until False;
  while lb4CANMsg.Count>FListBufLen do
    lb4CANMsg.Items.Delete(0);
  lb4CANMsg.Items.EndUpdate;
end;

procedure TForm1.HIDDataIn(buf: PByte; cnt: Integer); // Процедура обрабатывает приход данных от USB устройства
var
  candata:PCANData;
  len,i,j:Byte;
  strdec,strhex:string[200];
  idf:Integer;
  ignore:Boolean;
begin
  for j:=0 to cnt div 10-1 do begin
    candata:=@buf[1+j*10];  // Адрес на начало j-го сообщения, которое может прийти по шине CAN в течении 1 мс

    len:=candata^.id shr 11; // в битах 15-11 находится кол-во байт данных, пришедших по CAN шине
    if len<>0 then begin // Если кол-во данных не равно нулю, значит есть, что считывать
      candata^.id:=candata^.id and $fff;

      ignore:=False;
      for idf in FFiltrList do begin
        if idf=candata^.id then begin ignore:=True; Break; end;
      end;
      if ignore then Continue;

      strdec:=Format('0x%.3x   ',[candata^.id]);
      strhex:='';

      for i:=0 to len-1 do begin
        strdec:=strdec+Format('    %.3d',[candata^.data.bts[i]]);
        strhex:=strhex+Format('    0x%.2x',[candata^.data.bts[i]]);;
      end;
      strdec:=strdec+'     '+strhex;

      if not FStrRingBuff.CanWrite then Continue; // Если нет возможности писать в кольцевой буфер, то делать тут нечего
      FCANMassege[FStrRingBuff.Index4Write]:=strdec;
      FStrRingBuff.EndWrite;  // Обязательно нужно запустить процедуру, которая закрывает запись для нормальной работы кольцевого буфера
    end;
  end;
end;

procedure TForm1.NoConnectWithDev(Sender: TObject);
begin
  FConnectBreak:=True;
end;

procedure TForm1.LoadSendsMessages(fname: string);
var
  i:Integer;
  str:string;
  ftxt:TextFile;
begin
  AssignFile(ftxt, fname);
  Reset(ftxt);

  for i:=1 to StringGrid1.RowCount-1 do begin
    ReadLn(ftxt, str);
    StringGrid1.Rows[i].CommaText:=str;
  end;
  CloseFile(ftxt);
end;

procedure TForm1.SaveSendsMessages(fname: string);
var
  i:Integer;
  ftxt:TextFile;
begin
  AssignFile(ftxt, fname);
  Rewrite(ftxt);
  for i:=1 to StringGrid1.RowCount-1 do begin
    WriteLn(ftxt, StringGrid1.Rows[i].CommaText);
  end;
  CloseFile(ftxt);
end;

procedure TForm1.SaveRevsMessages(fname: string);
var
  i:Integer;
  ftxt:TextFile;
begin
  AssignFile(ftxt, fname);
  Rewrite(ftxt);

  for i:=0 to lb4CANMsg.Items.Count-1 do
    WriteLn(ftxt, lb4CANMsg.Items.Strings[i]);

  CloseFile(ftxt);
end;

procedure TForm1.btnOpenDevClick(Sender: TObject);
begin
  if FUSBWorker.OpenUSBDByName(cbDeviceList.Caption) then begin
    FStrRingBuff.Clear;
    FRecCount:=0;
    s4ShowConnectStatus.Brush.Color:=clGreen;
    FConnectBreak:=False;
    tm4UpdateMsgList.Enabled:=True;
  end else begin
    s4ShowConnectStatus.Brush.Color:=clRed;
    ShowMessage('Ошибка при открытии');
  end;
end;

procedure TForm1.btnSendRowClick(Sender: TObject);
var
  cd:PCANData;
  id,len:Integer;
begin
  FUSBWorker.SendBuf[0]:=1;
  cd:=@FUSBWorker.SendBuf[1];

  id:=StringGrid1.Cells[1, FSendRow].ToInteger;   // Считываем идентификатор CAN сообщения
  len:=StringGrid1.Cells[2, FSendRow].ToInteger;  // Считываем длину для CAN сообщения

  cd^.id:=(id and $7ff) or (len shl 11);
  for len:=0 to len-1 do
    cd^.data.bts[len]:=StringGrid1.Cells[3+len, FSendRow].ToInteger;;

  if FUSBWorker.SendData(20)<0 then
    ShowMessage('Ошибка при отправке данных');
end;

procedure TForm1.cbFiltrCANIDListKeyDown(Sender: TObject; var Key: Word; Shift: TShiftState);
var
  val:Integer;
begin
  if Key=46 then begin
    val:=cbFiltrCANIDList.Items.IndexOf(cbFiltrCANIDList.Caption);
    if val>=0 then
      cbFiltrCANIDList.Items.Delete(val);
  end;
end;

procedure TForm1.cbFiltrCANIDListKeyPress(Sender: TObject; var Key: char);
var
  val,n:Integer;
begin
  if Key=#13 then begin
    if not TryStrToInt(cbFiltrCANIDList.Text, val) then Exit;
    cbFiltrCANIDList.Items.Add(cbFiltrCANIDList.Text);
    n:=Length(FFiltrList);
    SetLength(FFiltrList, n+1);
    FFiltrList[n]:=val;
  end;
end;

procedure TForm1.enBufLenKeyPress(Sender: TObject; var Key: char);
var
  l:Integer;
begin
  if Key=#13 then begin
    l:=enBufLen.TxtToInt;
    if l>0 then FListBufLen:=l;
  end;
end;

procedure TForm1.FormClose(Sender: TObject; var CloseAction: TCloseAction);
begin
  btnCloseDevClick(nil); // Закрываем устройство
  SaveSendsMessages('DefaultsSendsCANMsg.txt'); // Сохранение отправляемых сообщений в файл по умолчанию
end;

procedure TForm1.btnCloseDevClick(Sender: TObject);
begin
  tm4UpdateMsgList.Enabled:=False;
  FUSBWorker.CloseHidDevice; // Закрытие устройства
end;

procedure TForm1.btnGetDevListClick(Sender: TObject);
begin
  cbDeviceList.Items.Clear;
  cbDeviceList.Items.AddStrings(FUSBWorker.GetUSBDeviceNames(22276, 1155));
  if cbDeviceList.Items.Count<>0 then cbDeviceList.ItemIndex:=0;
end;

procedure TForm1.btnClearClick(Sender: TObject);
begin
  lb4CANMsg.Items.Clear;
end;

procedure TForm1.btnClearFiltrCANIDListClick(Sender: TObject);
begin
  cbFiltrCANIDList.Clear;
  FFiltrList:=nil;
end;

end.

