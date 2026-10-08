// Read-only, bounded report for native player HUD ownership and data binding.
// Run on the supported program with analyzeHeadless -readOnly -noanalysis.
// Keep generated decompilation output private; this script contains no assets.
// @category SudekiMP
import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.listing.InstructionIterator;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.symbol.ReferenceIterator;

public class DevPlayHudBindingReport extends GhidraScript {
    private static final String SHA256 =
        "8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94";

    private void references(long value) throws Exception {
        Address address = toAddr(value);
        ReferenceIterator refs = currentProgram.getReferenceManager().getReferencesTo(address);
        int count = 0;
        while (refs.hasNext()) {
            monitor.checkCancelled();
            if (++count > 128) throw new Exception("reference bound exceeded at " + address);
            Reference ref = refs.next();
            println("REFERENCE " + address + " <- " + ref.getFromAddress()
                + " owner=" + getFunctionContaining(ref.getFromAddress()));
        }
    }

    public void run() throws Exception {
        if (currentProgram == null
            || !SHA256.equalsIgnoreCase(currentProgram.getExecutableSHA256())
            || currentProgram.getImageBase().getOffset() != 0x400000L)
            throw new Exception("unsupported image");
        println("SHA256=" + currentProgram.getExecutableSHA256());
        DecompInterface decompiler = new DecompInterface();
        if (!decompiler.openProgram(currentProgram))
            throw new Exception("cannot open decompiler");
        try {
            long[] entries = {
                // Native layer, four resident gizmos, readiness, teardown.
                0x4a56f0L, 0x4a57f0L, 0x580b10L, 0x581260L, 0x581390L,
                0x581470L, 0x581930L, 0x4a9060L, 0x4a94f0L,
                0x4aa170L, 0x4a95e0L,
                // Display state, visibility, broad refresh and selection.
                0x4a5930L, 0x4aa010L, 0x4aa910L, 0x4aaa90L,
                0x4a6450L, 0x4a6700L,
                // Numeric values, ratios, character name, tactic label/status.
                0x5814e0L, 0x4a9d40L, 0x4a9de0L, 0x4a9cd0L,
                0x52bb60L, 0x4aac90L, 0x582230L,
                // Intrusive non-owning actor link and portrait type mapping.
                0x4015b0L, 0x4015e0L, 0x43f430L, 0x4aab00L,
                // Numeric resource request, material delivery and UI state.
                0x55c070L, 0x55c0e0L, 0x55c190L, 0x55c230L,
                0x55c270L, 0x55c2c0L,
                // Typed ResourceName parsing, ID generation and release.
                0x5b9440L, 0x5b96f0L, 0x5b9760L,
                // Scene-owned text queue and native bar/visibility binding.
                0x409810L, 0x409930L, 0x5b9fc0L, 0x581f90L,
                0x55be70L, 0x55c020L, 0x55c2d0L
            };
            for (long entry : entries) {
                monitor.checkCancelled();
                Function function = getFunctionAt(toAddr(entry));
                if (function == null) throw new Exception("missing function at " + toAddr(entry));
                println("FUNCTION " + function.getEntryPoint() + " " + function.getName(true));
                DecompileResults result = decompiler.decompileFunction(function, 60, monitor);
                if (!result.decompileCompleted())
                    throw new Exception("decompile failed: " + function + ": " + result.getErrorMessage());
                println(result.getDecompiledFunction().getC());
            }
            for (long entry : new long[] {0x409930L, 0x582230L, 0x55c070L, 0x55c0e0L}) {
                Function function = getFunctionAt(toAddr(entry));
                InstructionIterator instructions = currentProgram.getListing().getInstructions(function.getBody(), true);
                int count = 0;
                while (instructions.hasNext()) {
                    monitor.checkCancelled();
                    if (++count > 512) throw new Exception("ABI instruction bound exceeded");
                    Instruction instruction = instructions.next();
                    println("NATIVE_ABI " + instruction.getAddress() + " " + instruction);
                }
            }
            // Actor-copy/name seams and the native adapter's eight call sites.
            for (long call : new long[] {0x581517L, 0x4a9d5bL, 0x4a9e15L,
                                        0x4aacabL, 0x4a9eb5L,
                                        0x4a97cbL, 0x4a9608L, 0x4a97b7L, 0x4a5ffeL,
                                        0x4a5973L, 0x4aa965L, 0x55b92cL, 0x581e43L}) {
                Address start = toAddr(call);
                InstructionIterator instructions = currentProgram.getListing().getInstructions(start, true);
                int count = 0;
                while (instructions.hasNext() && count++ < 5) {
                    Instruction instruction = instructions.next();
                    println("SEAM " + instruction.getAddress() + " " + instruction);
                }
                if (count == 0) throw new Exception("missing seam at " + start);
            }
            for (int index = 0; index < 16; index++)
                println("PORTRAIT_ENUM " + index + " resource_key="
                    + Integer.toHexString(getInt(toAddr(0x6c2a94L + 4L * index))));
            for (long table : new long[] {0x6d8fb4L, 0x6d9004L, 0x6cb59cL})
                for (int index = 0; index < 7; index++)
                    println("HUD_VIRTUAL " + toAddr(table) + " slot=" + index
                        + " target=" + toAddr(Integer.toUnsignedLong(getInt(toAddr(table + 4L * index)))));
            // Known binding keys, including AI-labelled anchors; names alone
            // do not establish the meaning or owner of a rendered badge.
            for (int key : new int[] {0x91, 0x92, 0x93, 0x95, 0x98, 0x99,
                                      0x9a, 0x9b, 0x9d, 0x9f, 0xa1, 0xa3,
                                      0xa4, 0x127, 0x128, 0x129, 0x12a, 0x12b}) {
                Address entry = toAddr(0x6c3010L + 4L * key);
                Address value = toAddr(Integer.toUnsignedLong(getInt(entry)));
                println("UI_KEY " + Integer.toHexString(key) + " " + value
                    + " " + getDataAt(value));
            }
            for (long value : new long[] {0x7c2f9cL, 0x6d7a08L, 0x6d79fcL,
                                         0x6c3278L, 0x6c327cL,
                                         0x4a9d40L, 0x4aac90L, 0x4aab00L})
                references(value);
            println("DEV_PLAY_HUD_BINDING_REPORT_COMPLETE");
        } finally {
            decompiler.dispose();
        }
    }
}
