// Read-only title portrait residency and synchronous resource ownership.
// @category SudekiMP
import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.DecompInterface;
import ghidra.program.model.listing.Function;
public class DevPlayTitlePortraitReport extends GhidraScript {
    public void run() throws Exception {
        if(!"8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94".equalsIgnoreCase(currentProgram.getExecutableSHA256()))
            throw new Exception("unsupported image");
        DecompInterface d=new DecompInterface();d.openProgram(currentProgram);
        try {
            for(long entry:new long[]{0x480480L,0x4810a0L,0x481830L,0x4806b0L,0x480060L,
                0x484130L,0x484260L,0x43f430L,0x55c0e0L,0x55c270L,0x5d92e0L,0x5d6810L,
                0x5d91b0L,0x5d8d70L,0x5d9630L,0x5d9800L,0x5d6730L,0x55bd10L,0x55be70L,
                0x4805f0L,0x480660L,0x4806f0L,0x4804a0L,0x5d6a90L,0x5d6b30L,
                0x5d6c70L,0x5d7000L,0x5d7320L,0x5d9af0L,0x5f54d0L}) {
                Function f=getFunctionAt(toAddr(entry));println("entry="+toAddr(entry)+" function="+f);
                if(f==null) continue;
                var r=d.decompileFunction(f,90,monitor);
                println(r.decompileCompleted()?r.getDecompiledFunction().getC():r.getErrorMessage());
            }
        } finally {d.dispose();}
    }
}
