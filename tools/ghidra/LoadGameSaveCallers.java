// Read-only exact-image audit for the final native LoadGameSave boundary.
import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.symbol.ReferenceIterator;

public class LoadGameSaveCallers extends GhidraScript {
  private static final String SHA="8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94";
  private Address a(long v){return currentProgram.getAddressFactory().getDefaultAddressSpace().getAddress(v);}
  private void dump(DecompInterface d,long v,String role){
    Function f=currentProgram.getFunctionManager().getFunctionAt(a(v));
    println("\n==== "+role+" "+a(v)+" ====");
    if(f==null){println("missing=true");return;}
    DecompileResults r=d.decompileFunction(f,240,monitor);
    println(r.decompileCompleted()?r.getDecompiledFunction().getC():r.getErrorMessage());
  }
  private void callers(DecompInterface d,long v){
    ReferenceIterator it=currentProgram.getReferenceManager().getReferencesTo(a(v)); int n=0;
    println("\n==== callers "+a(v)+" ====");
    while(it.hasNext()&&!monitor.isCancelled()){
      Reference r=it.next(); if(!r.getReferenceType().isCall())continue;
      Function f=currentProgram.getFunctionManager().getFunctionContaining(r.getFromAddress());
      println("callsite="+r.getFromAddress()+" owner="+(f==null?"<none>":f.getEntryPoint()));
      if(f!=null)dump(d,f.getEntryPoint().getOffset(),"caller");
      if(++n>=40)break;
    }
    println("callers="+n);
  }
  public void run() throws Exception{
    if(!SHA.equalsIgnoreCase(currentProgram.getExecutableSHA256()))throw new Exception("unexpected image");
    DecompInterface d=new DecompInterface();d.openProgram(currentProgram);
    dump(d,0x00501690L,"LoadGameSave"); callers(d,0x00501690L); d.dispose();
  }
}
