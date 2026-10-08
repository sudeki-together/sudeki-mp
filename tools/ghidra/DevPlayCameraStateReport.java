// Read-only camera state callback identity; no program edits.
// @category SudekiMP
import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.DecompInterface;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
public class DevPlayCameraStateReport extends GhidraScript {
    public void run() throws Exception {
        if(!"8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94".equalsIgnoreCase(currentProgram.getExecutableSHA256()))
            throw new Exception("unsupported image");
        DecompInterface d=new DecompInterface(); d.openProgram(currentProgram);
        try {
            long[] tables={0x6d9854L,0x6d98e4L,0x6d9984L};
            String[] states={"Exploration","Combat","BossCombat"};
            for(int state=0;state<tables.length;state++) {
                Address v=toAddr(tables[state]);
                println("state="+states[state]+" vtable="+v);
                for(int slot=0;slot<=0x40;slot+=4) {
                    Address a=toAddr(Integer.toUnsignedLong(getInt(v.add(slot))));
                    Function f=getFunctionAt(a); println("slot="+slot+" address="+a+" function="+f);
                    if(slot==0x40 && f!=null) {
                        var r=d.decompileFunction(f,60,monitor);
                        println(r.decompileCompleted()?r.getDecompiledFunction().getC():r.getErrorMessage());
                    }
                }
            }
            for(long entry:new long[]{
                    0x47abb0L,0x58c430L,0x58c700L,0x58c750L,0x58c7a0L,
                    0x47c280L,0x59e2f0L,0x5a3050L,0x59cde0L,
                    0x4e79a0L,0x4e84c0L,0x47bb90L,0x534610L,
                    0x436890L,0x534fb0L,
                    // Generic actor-follow target, owned offset and native tick.
                    0x534b30L,0x535040L,0x535100L,0x59a290L,
                    0x59a300L,0x59a500L,0x437170L,0x4e7660L,
                    0x47cbb0L,0x47c2c0L,0x47ccd0L,0x47c190L}) {
                Function f=getFunctionAt(toAddr(entry));
                println("dependency="+toAddr(entry)+" function="+f);
                if(f==null) continue;
                var r=d.decompileFunction(f,60,monitor);
                println(r.decompileCompleted()?r.getDecompiledFunction().getC():r.getErrorMessage());
            }
        } finally { d.dispose(); }
    }
}
