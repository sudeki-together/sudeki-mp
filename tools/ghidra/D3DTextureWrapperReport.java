// Read-only exact-build report for Sudeki's cD3DTexture wrapper.
// @category SudekiMP
import ghidra.app.decompiler.*;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.*;
import ghidra.program.model.mem.Memory;
import ghidra.program.model.symbol.*;
import java.util.*;
public class D3DTextureWrapperReport extends GhidraScript {
 private static final String SHA="8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94";
 private void dump(DecompInterface dc,Function f,Set<Address>s){if(f==null||!s.add(f.getEntryPoint()))return;println("\n===== "+f.getEntryPoint()+" "+f.getName(true)+" =====");DecompileResults r=dc.decompileFunction(f,120,monitor);println(r.decompileCompleted()?r.getDecompiledFunction().getC():r.getErrorMessage());}
 @Override public void run() throws Exception{if(!SHA.equalsIgnoreCase(currentProgram.getExecutableSHA256()))throw new Exception("unexpected image");DecompInterface dc=new DecompInterface();dc.openProgram(currentProgram);Set<Address>s=new HashSet<Address>();SymbolIterator it=currentProgram.getSymbolTable().getAllSymbols(true);while(it.hasNext()&&!monitor.isCancelled()){Symbol x=it.next();if(!x.getName(true).contains("cD3DTexture"))continue;println("SYMBOL address="+x.getAddress()+" name="+x.getName(true)+" type="+x.getSymbolType());Address a=x.getAddress();try{for(int i=0;i<24;i++){Address p=a.add(i*4);int v=currentProgram.getMemory().getInt(p);println("  word["+i+"]="+String.format("0x%08x",v));Function f=currentProgram.getFunctionManager().getFunctionAt(currentProgram.getAddressFactory().getDefaultAddressSpace().getAddress(Integer.toUnsignedLong(v)));dump(dc,f,s);}}catch(Exception e){println("  vtable_read_error="+e.getMessage());}}dc.dispose();}
}
