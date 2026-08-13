// Reports the native missile launch and impact path for Phase 5.
// The script is read-only and refuses to inspect an unexpected executable.
// @category SudekiMP

import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.FunctionIterator;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.symbol.ReferenceIterator;
import ghidra.program.model.symbol.Symbol;
import ghidra.program.model.symbol.SymbolIterator;

import java.util.LinkedHashSet;
import java.util.Set;

public class MissileImpactReport extends GhidraScript {
    private static final String EXPECTED_SHA256 =
        "8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94";

    private Address address(long value) {
        return currentProgram.getAddressFactory().getDefaultAddressSpace().getAddress(value);
    }

    private void reportFunctions(long start, long end) {
        println(String.format("Functions in [%08X, %08X]:", start, end));
        FunctionIterator functions = currentProgram.getFunctionManager().getFunctions(address(start), true);
        while (functions.hasNext()) {
            Function function = functions.next();
            long entry = function.getEntryPoint().getOffset();
            if (entry > end) {
                break;
            }
            println(String.format("  %s-%s  %s", function.getEntryPoint(),
                function.getBody().getMaxAddress(), function.getName(true)));
        }
    }

    private void reportSymbols(String needle) {
        println("Symbols containing '" + needle + "':");
        SymbolIterator symbols = currentProgram.getSymbolTable().getAllSymbols(true);
        int count = 0;
        while (symbols.hasNext()) {
            Symbol symbol = symbols.next();
            if (symbol.getName(true).toLowerCase().contains(needle.toLowerCase())) {
                println("  " + symbol.getAddress() + "  " + symbol.getName(true));
                count++;
            }
        }
        println("  count=" + count);
    }

    private Set<Function> reportReferences(long value, String role) {
        Address target = address(value);
        ReferenceIterator references = currentProgram.getReferenceManager().getReferencesTo(target);
        Set<Function> callers = new LinkedHashSet<>();
        int count = 0;
        println(role + " target=" + target);
        while (references.hasNext()) {
            Reference reference = references.next();
            Function caller = currentProgram.getFunctionManager().getFunctionContaining(
                reference.getFromAddress()
            );
            println(String.format("  from=%s type=%s function=%s",
                reference.getFromAddress(), reference.getReferenceType(),
                caller == null ? "<none>" : caller.getName(true)));
            if (caller != null) {
                callers.add(caller);
            }
            count++;
        }
        println("  references=" + count + " distinct_functions=" + callers.size());
        return callers;
    }

    private void decompile(DecompInterface decompiler, Function function, String role) {
        DecompileResults result = decompiler.decompileFunction(function, 90, monitor);
        println("");
        println("===== " + role + " " + function.getEntryPoint() + " " +
            function.getName(true) + " =====");
        if (!result.decompileCompleted()) {
            println("Decompiler failed: " + result.getErrorMessage());
            return;
        }
        println(result.getDecompiledFunction().getC());
    }

    private void decompile(DecompInterface decompiler, long value, String role) {
        Function function = currentProgram.getFunctionManager().getFunctionAt(address(value));
        if (function == null) {
            println(role + ": no function at " + address(value));
            return;
        }
        decompile(decompiler, function, role);
    }

    private void decompileMissileCallers(
        DecompInterface decompiler,
        Set<Function> functions,
        String role
    ) {
        for (Function function : functions) {
            long entry = function.getEntryPoint().getOffset();
            if (entry >= 0x004c0000L && entry < 0x004d0000L) {
                decompile(decompiler, function, role);
            }
        }
    }

    @Override
    protected void run() throws Exception {
        String actualSha256 = currentProgram.getExecutableSHA256();
        if (!EXPECTED_SHA256.equalsIgnoreCase(actualSha256)) {
            throw new Exception("Unexpected executable SHA256: " + actualSha256);
        }

        println("SudekiMP native missile impact report");
        println("SHA256=" + actualSha256);
        println("ImageBase=" + currentProgram.getImageBase());
        println("");

        reportSymbols("Missile");
        reportSymbols("HitEntity");
        reportSymbols("DoDirectDamage");
        reportSymbols("ModifyHitPoints");
        reportFunctions(0x004c5000L, 0x004c9000L);
        reportFunctions(0x00585000L, 0x00588000L);
        reportFunctions(0x0053d000L, 0x00543000L);
        reportFunctions(0x004dc000L, 0x004df500L);

        Set<Function> hitEntityCallers = reportReferences(
            0x004a2900L, "HitEntity"
        );
        Set<Function> directDamageCallers = reportReferences(
            0x004d3ae0L, "CCombat::DoDirectDamage"
        );
        Set<Function> modifyHitPointsCallers = reportReferences(
            0x004dc4f0L, "CCharacterArbiter::ModifyHitPoints"
        );

        DecompInterface decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);
        decompile(decompiler, 0x004c6de0L, "CMissileManager select/prepare missile");
        decompile(decompiler, 0x004c70f0L, "CMissileManager nearby helper");
        decompile(decompiler, 0x004c7160L, "CMissileManager launch missile");
        decompile(decompiler, 0x004c77b0L, "CMissileManager nearby helper");
        decompile(decompiler, 0x004c7930L, "CMissileManager nearby helper");
        decompile(decompiler, 0x004c79c0L, "CMissileManager nearby helper");
        decompile(decompiler, 0x004c7aa0L, "CMissileManager nearby helper");
        decompile(decompiler, 0x004c7d20L, "CMissileManager nearby helper");
        decompile(decompiler, 0x004c87b0L, "CMissileManager nearby helper");
        decompile(decompiler, 0x004c89c0L, "CMissileManager::FireMissileScripted");
        decompile(decompiler, 0x004c8a40L, "CMissileManager nearby helper");
        decompile(decompiler, 0x005862b0L, "CMissile vtable target 5862B0");
        decompile(decompiler, 0x005861c0L, "CMissile construction helper");
        decompile(decompiler, 0x005862f0L, "CMissile destruction helper");
        decompile(decompiler, 0x00586310L, "CMissile nearby helper 586310");
        decompile(decompiler, 0x00586470L, "CMissile vtable target 586470");
        decompile(decompiler, 0x00586560L, "CMissile vtable target 586560");
        decompile(decompiler, 0x00586600L, "CMissile vtable target 586600");
        decompile(decompiler, 0x00586610L, "CMissile terminate/impact helper");
        decompile(decompiler, 0x005867d0L, "CMissile collision/damage helper");
        decompile(decompiler, 0x00586c90L, "CMissile cleanup helper");
        decompile(decompiler, 0x00586e10L, "CMissile initialization helper");
        decompile(decompiler, 0x005872f0L, "CMissile vtable target 5872F0");
        decompile(decompiler, 0x00587300L, "CMissile vtable target 587300");
        decompile(decompiler, 0x00587350L, "CMissile vtable target 587350");
        decompile(decompiler, 0x00587380L, "CMissile vtable target 587380");
        decompile(decompiler, 0x00587950L, "CMissile vtable target 587950");
        decompile(decompiler, 0x00587960L, "CMissile vtable target 587960");
        decompile(decompiler, 0x00587600L, "StandardMissileMovementController vtable target");
        decompile(decompiler, 0x00587390L, "Missile movement controller setup");
        decompile(decompiler, 0x0058c390L, "SpiralMissileMovementController vtable target");
        decompile(decompiler, 0x00587710L, "BouncingMissileMovementController vtable target");
        decompile(decompiler, 0x00587970L, "MissileData construction helper");
        decompile(decompiler, 0x00587a80L, "MissileData serialization helper");
        decompile(decompiler, 0x004dcd00L, "Native missile collision submission");
        decompile(decompiler, 0x004de540L, "Collision resolver helper 4DE540");
        decompile(decompiler, 0x004de920L, "Collision resolver helper 4DE920");
        decompile(decompiler, 0x004df0f0L, "Collision resolver helper 4DF0F0");
        decompile(decompiler, 0x00541af0L, "MissileEntity vtable target 541AF0");
        decompile(decompiler, 0x00541d30L, "MissileEntity vtable target 541D30");
        decompile(decompiler, 0x00541e00L, "MissileEntity vtable target 541E00");
        decompile(decompiler, 0x00541ee0L, "MissileEntity vtable target 541EE0");
        decompile(decompiler, 0x00541f50L, "MissileEntity vtable target 541F50");
        decompile(decompiler, 0x00541f60L, "MissileEntity vtable target 541F60");
        decompile(decompiler, 0x0053dd40L, "MissileEntity vtable target 53DD40");
        decompile(decompiler, 0x0053dd80L, "MissileEntity vtable target 53DD80");
        decompile(decompiler, 0x0053ded0L, "MissileEntity vtable target 53DED0");
        decompile(decompiler, 0x0053dfa0L, "MissileEntity vtable target 53DFA0");
        decompile(decompiler, 0x0053e070L, "MissileEntity vtable target 53E070");
        decompile(decompiler, 0x0053e080L, "MissileEntity vtable target 53E080");
        decompile(decompiler, 0x0053e0c0L, "MissileEntity vtable target 53E0C0");
        decompile(decompiler, 0x0053e190L, "MissileEntity vtable target 53E190");
        decompile(decompiler, 0x0053e1c0L, "MissileEntity vtable target 53E1C0");
        decompileMissileCallers(decompiler, hitEntityCallers, "Missile-range HitEntity caller");
        decompileMissileCallers(decompiler, directDamageCallers,
            "Missile-range DoDirectDamage caller");
        decompileMissileCallers(decompiler, modifyHitPointsCallers,
            "Missile-range ModifyHitPoints caller");
        decompiler.dispose();
    }
}
