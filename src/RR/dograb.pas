{$m 4000,0,0}
uses dos;
function KeyPressed:boolean; inline($b4/$0b/$cd/$21);{mov ah,0bh;int 21h}
var i,f,s,er:word;
    code:integer;
    st:string;
begin
 val(paramstr(2),f,code);
 if (paramcount<3) or (f=0) or (code<>0) then begin
   writeln('pouziti: grab jidelna 20 10 [-nogfx] [-l]');
   writeln('  ...z jidelna.bsp nagrabuje 20 snimku po 10 sekundach pocitani');
   halt;
   end;
 for i:=0 to f-1 do begin
   str(i,st);
   exec('rr.exe',paramstr(1)+'.bsp -f'+paramstr(2)+' -s'+paramstr(3)+' -g'+st+
         ' '+paramstr(4)+' '+paramstr(5)+' '+paramstr(6));
   er:=doserror;if er<>0 then begin writeln('doserror=',er);halt(1);end;
   er:=dosexitcode; if er<>0 then begin WriteLn('exitcode=',er);halt(1);end;
   if keypressed then begin writeln('aborted by anykey');halt(1);end;
   end;
 writeln('hotovo');
end.
