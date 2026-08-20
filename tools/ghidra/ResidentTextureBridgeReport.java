// Read-only exact-build report for Sudeki's resident D3D texture layer.
// Goal: establish an exact, safe path from cTexture to the underlying D3D9
// texture (or prove that the title overlay must use a native draw path).
// @category SudekiMP
import ghidra.app.decompiler.*;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.*;
import ghidra.program.model.symbol.*;
import java.util.*;
public class ResidentTextureBridgeReport extends GhidraScript {
  private static final String SHA="8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94";
  private Address a(long x){return currentProgram.getAddressFactory().getDefaultAddressSpace().getAddress(x);}
  private void dump(DecompInterface d,Function f,String label,Set<Address> seen){
    println("\n===== "+label+" "+(f==null?"<missing>":f.getEntryPoint()+" "+f.getName(true))+" =====");
    if(f==null||!seen.add(f.getEntryPoint()))return;
    DecompileResults r=d.decompileFunction(f,180,monitor);
    println(r.decompileCompleted()?r.getDecompiledFunction().getC():r.getErrorMessage());
  }
  private void callers(DecompInterface d,long x,String label,Set<Address>seen){
    println("\n===== CALLERS "+label+" "+a(x)+" =====");int n=0;
    ReferenceIterator it=currentProgram.getReferenceManager().getReferencesTo(a(x));
    while(it.hasNext()&&!monitor.isCancelled()){Reference r=it.next();if(!r.getReferenceType().isCall())continue;
      Function f=currentProgram.getFunctionManager().getFunctionContaining(r.getFromAddress());
      println("callsite="+r.getFromAddress()+" owner="+(f==null?"<none>":f.getEntryPoint()));dump(d,f,label+" caller",seen);if(++n>=20)break;}
    println("callers="+n);
  }
  private void vtable(DecompInterface d,String needle,Set<Address>seen){
    SymbolIterator it=currentProgram.getSymbolTable().getAllSymbols(true);
    while(it.hasNext()&&!monitor.isCancelled()){Symbol s=it.next();if(!s.getName(true).equals(needle))continue;
      println("\n===== VTABLE "+needle+" at "+s.getAddress()+" =====");
      for(int i=0;i<24;i++){try{int raw=currentProgram.getMemory().getInt(s.getAddress().add(i*4));
        Address target=a(Integer.toUnsignedLong(raw));Function f=currentProgram.getFunctionManager().getFunctionAt(target);
        println("slot="+i+" target="+target+" function="+(f==null?"<none>":f.getName(true)));dump(d,f,needle+" slot "+i,seen);}catch(Exception e){println("read_error="+e.getMessage());break;}}
    }
  }
  @Override public void run() throws Exception{
    if(!SHA.equalsIgnoreCase(currentProgram.getExecutableSHA256()))throw new Exception("unexpected image");
    DecompInterface d=new DecompInterface();d.openProgram(currentProgram);Set<Address>seen=new HashSet<Address>();
    vtable(d,"cResidentD3DTexture::vftable",seen);
    vtable(d,"cD3DTexture::vftable",seen);
    long[] xs={0x005d9800L,0x005d9af0L,0x005d9b80L,0x005d9630L,0x005d6730L};
    String[] names={"d3d_texture_render_or_bind","d3d_texture_resident_release","d3d_texture_global_cleanup","d3d_texture_release","d3d_texture_clone_job"};
    for(int i=0;i<xs.length;i++){dump(d,currentProgram.getFunctionManager().getFunctionAt(a(xs[i])),names[i],seen);callers(d,xs[i],names[i],seen);}
    d.dispose();
  }
}
