// List symbols whose names contain a supplied case-insensitive substring.
// @category XeO3

import java.io.File;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.util.ArrayList;
import java.util.Comparator;
import java.util.List;
import java.util.Locale;

import ghidra.app.script.GhidraScript;
import ghidra.program.model.symbol.Symbol;
import ghidra.program.model.symbol.SymbolIterator;

public class ListSymbolsMatching extends GhidraScript {
    @Override
    protected void run() throws Exception {
        String[] arguments = getScriptArgs();
        if (arguments.length != 2) {
            throw new IllegalArgumentException(
                "Usage: ListSymbolsMatching <substring> <output-file>");
        }

        String needle = arguments[0].toLowerCase(Locale.ROOT);
        List<Symbol> matches = new ArrayList<>();
        SymbolIterator symbols = currentProgram.getSymbolTable().getAllSymbols(true);
        while (symbols.hasNext()) {
            Symbol symbol = symbols.next();
            if (symbol.getName(true).toLowerCase(Locale.ROOT).contains(needle)) {
                matches.add(symbol);
            }
        }
        matches.sort(Comparator
            .comparing((Symbol symbol) -> symbol.getAddress())
            .thenComparing(symbol -> symbol.getName(true)));

        StringBuilder output = new StringBuilder();
        for (Symbol symbol : matches) {
            output.append(symbol.getAddress())
                .append('\t')
                .append(symbol.getSymbolType())
                .append('\t')
                .append(symbol.getName(true))
                .append(System.lineSeparator());
        }
        Files.writeString(new File(arguments[1]).toPath(), output.toString(),
            StandardCharsets.UTF_8);
        println("Found " + matches.size() + " symbols containing " + arguments[0]);
    }
}
