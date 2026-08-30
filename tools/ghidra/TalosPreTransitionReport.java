// Read-only exact-build report for the final Talos pre-SetZoneNOW seam.
// @category SudekiMP

import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.mem.Memory;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.symbol.ReferenceIterator;

public class TalosPreTransitionReport extends GhidraScript {
    private static final String EXPECTED_SHA256 =
        "8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94";

    private Address address(long value) {
        return currentProgram.getAddressFactory().getDefaultAddressSpace()
            .getAddress(value);
    }

    private void reportTarget(DecompInterface decompiler, long value,
            String label) throws Exception {
        Function target = currentProgram.getFunctionManager()
            .getFunctionAt(address(value));
        println("===== " + label + " target=" +
            (target == null ? "<missing>" : target.getEntryPoint() + " " +
            target.getName(true)) + " =====");
        if (target == null) return;
        ReferenceIterator references = currentProgram.getReferenceManager()
            .getReferencesTo(target.getEntryPoint());
        while (references.hasNext() && !monitor.isCancelled()) {
            Reference reference = references.next();
            println("REF from=" + reference.getFromAddress() + " type=" +
                reference.getReferenceType());
            if (!reference.getReferenceType().isCall()) continue;
            Function caller = currentProgram.getFunctionManager()
                .getFunctionContaining(reference.getFromAddress());
            println("CALLER=" + (caller == null ? "<none>" :
                caller.getEntryPoint() + " " + caller.getName(true)));
            if (caller != null) {
                DecompileResults result = decompiler.decompileFunction(
                    caller, 90, monitor);
                if (result.decompileCompleted()) {
                    println(result.getDecompiledFunction().getC());
                }
            }
        }
    }

    private void reportVoidReferences() throws Exception {
        Memory memory = currentProgram.getMemory();
        byte[] pattern = new byte[] { 'V', 'o', 'i', 'd', 0 };
        Address cursor = memory.getMinAddress();
        println("===== ASCII Void references =====");
        while (cursor != null && !monitor.isCancelled()) {
            Address hit = memory.findBytes(cursor, pattern, null, true, monitor);
            if (hit == null) break;
            println("VOID_STRING at=" + hit);
            ReferenceIterator refs = currentProgram.getReferenceManager()
                .getReferencesTo(hit);
            while (refs.hasNext()) {
                Reference ref = refs.next();
                Function function = currentProgram.getFunctionManager()
                    .getFunctionContaining(ref.getFromAddress());
                println("VOID_REF from=" + ref.getFromAddress() + " type=" +
                    ref.getReferenceType() + " function=" +
                    (function == null ? "<none>" : function.getEntryPoint() +
                    " " + function.getName(true)));
            }
            cursor = hit.next();
        }
    }

    private void decompileContaining(DecompInterface decompiler, long value,
            String label) throws Exception {
        Address at = address(value);
        Function function = currentProgram.getFunctionManager()
            .getFunctionContaining(at);
        println("===== " + label + " at=" + at + " function=" +
            (function == null ? "<none>" : function.getEntryPoint() + " " +
            function.getName(true)) + " =====");
        if (function == null) return;
        DecompileResults result = decompiler.decompileFunction(
            function, 90, monitor);
        if (result.decompileCompleted()) {
            println(result.getDecompiledFunction().getC());
        }
    }

    private void reportWindow(long start, long end, String label)
            throws Exception {
        println("===== " + label + " instructions =====");
        Address cursor = address(start);
        Address stop = address(end);
        while (cursor.compareTo(stop) <= 0 && !monitor.isCancelled()) {
            Instruction instruction = currentProgram.getListing()
                .getInstructionAt(cursor);
            if (instruction == null) {
                cursor = cursor.next();
            } else {
                println(instruction.getAddress() + "  " + instruction);
                cursor = instruction.getMaxAddress().next();
            }
        }
    }

    private void reportDataReferences(long value, String label)
            throws Exception {
        Address target = address(value);
        println("===== " + label + " target=" + target + " =====");
        ReferenceIterator refs = currentProgram.getReferenceManager()
            .getReferencesTo(target);
        while (refs.hasNext() && !monitor.isCancelled()) {
            Reference ref = refs.next();
            Function function = currentProgram.getFunctionManager()
                .getFunctionContaining(ref.getFromAddress());
            println("DATA_REF from=" + ref.getFromAddress() + " type=" +
                ref.getReferenceType() + " function=" +
                (function == null ? "<none>" : function.getEntryPoint() +
                " " + function.getName(true)));
        }
    }

    private void reportOpcodeHandler(DecompInterface decompiler, int opcode,
            String label) throws Exception {
        long slotValue = 0x00723f04L + ((long)opcode * 4L);
        Address slot = address(slotValue);
        long handlerValue = Integer.toUnsignedLong(
            currentProgram.getMemory().getInt(slot));
        println("===== " + label + " opcode=0x" +
            Integer.toHexString(opcode) + " slot=" + slot + " handler=" +
            address(handlerValue) + " =====");
        Function handler = currentProgram.getFunctionManager()
            .getFunctionAt(address(handlerValue));
        if (handler == null) {
            println("HANDLER_FUNCTION=<missing>");
            return;
        }
        println("HANDLER_FUNCTION=" + handler.getEntryPoint() + " " +
            handler.getName(true));
        DecompileResults result = decompiler.decompileFunction(
            handler, 90, monitor);
        if (result.decompileCompleted()) {
            println(result.getDecompiledFunction().getC());
        }
    }

    @Override
    protected void run() throws Exception {
        if (!EXPECTED_SHA256.equalsIgnoreCase(
                currentProgram.getExecutableSHA256())) {
            throw new Exception("Unexpected executable SHA256: " +
                currentProgram.getExecutableSHA256());
        }
        DecompInterface decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);
        reportTarget(decompiler, 0x00407910L, "SetZoneNOW");
        decompileContaining(decompiler, 0x006352b1L,
            "runtime SetZoneNOW return site / binding dispatcher");
        reportWindow(0x00635280L, 0x006352d0L,
            "binding dispatcher native-call window");
        decompileContaining(decompiler, 0x005c4970L,
            "opcode 0x27 global/script call handler");
        reportOpcodeHandler(decompiler, 0x29,
            "opcode 0x29 LoadTheVoid-site handler");
        reportWindow(0x005c4d30L, 0x005c4e30L,
            "opcode 0x29 task-construction ABI window");
        decompileContaining(decompiler, 0x005c3170L,
            "opcode 0x29 task/table construction callee");
        decompileContaining(decompiler, 0x005c5aa0L,
            "opcode 0x29 argument materialization helper");
        reportDataReferences(0x00723fa0L,
            "opcode 0x27 pointer-hook slot");
        reportDataReferences(0x00723fa8L,
            "opcode 0x29 pointer-hook slot");
        reportDataReferences(0x0070d1dcL,
            "SetZoneNOW export-table slot");
        reportVoidReferences();
        println("===== exact companion SOLWORLDM.gex facts =====");
        println("ASSET_SHA256=" +
            "e36a5974f9aedea5b5b428fe2445cf496c52911ff01d4934ea8ab8124abf1ff9");
        println("BYTECODE_FILE_BASE raw=0x00027E6C logical=raw-base " +
            "anchors=IsPlaying(logical 0x3E38/raw 0x2BCA4)," +
            "FireMissileScripted(logical 0xAAE89/raw 0xD2CF5)");
        println("SOURCE action=CC_NPC_Caprine_TalkingT3|PP " +
            "hash=0xFAC73F18 logical_range=0x00021B76-0x00021D85 " +
            "opcode29=0x00021C0C(raw 0x00049A78) " +
            "task_hash=0x70F470C2");
        println("LOAD_VOID hash=0x70F470C2 " +
            "logical_range=0x000218F3-0x00021B08 " +
            "FMA07_hash=0xBC28D699 Void_hash=0x48B5725F " +
            "SetZone|S_opcode27=0x0002196E(raw 0x000497DA) " +
            "wrapper_hash=0x76FC7114");
        println("LOAD_VOID_SET_ZONE_RAW bytes@0x000497D5=" +
            "06 5F 72 B5 48 27 14 71 FC 76 " +
            "(Void operand ends 0x497D9; opcode begins 0x497DA)");
        println("SET_ZONE_WRAPPER hash=0x76FC7114 " +
            "logical_range=0x00003139-0x000031C2 " +
            "SetZoneNOW|S_opcode27=0x0000317C(raw 0x0002AFE8) " +
            "binding_hash=0xBC8FDC32");
        println("SAFETY production continuation remains disabled until a live " +
            "trace proves SOL task/call-stack ancestry across source opcode " +
            "0x29 -> LoadTheVoid -> SetZone|S opcode 0x27 -> native opcode " +
            "0x27; " +
            "RVA 0x002352B1 is only the generic native-binding return site.");
        println("ABI both opcode observations compete for mutable interpreter " +
            "slots; hooks must be centrally chained or rejected, must preserve " +
            "fastcall ECX/EDX, and must not retain raw thread/task pointers.");
        decompiler.dispose();
    }
}
