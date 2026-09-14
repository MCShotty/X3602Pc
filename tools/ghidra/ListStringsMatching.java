// List defined strings whose contents contain a case-insensitive substring.
// @category XeO3

import java.io.File;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.util.Locale;

import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.Data;
import ghidra.program.model.listing.DataIterator;

public class ListStringsMatching extends GhidraScript {
    @Override
    protected void run() throws Exception {
        String[] arguments = getScriptArgs();
        if (arguments.length != 2) {
            throw new IllegalArgumentException(
                "Usage: ListStringsMatching <substring> <output-file>");
        }

        String needle = arguments[0].toLowerCase(Locale.ROOT);
        StringBuilder output = new StringBuilder();
        int matchCount = 0;
        DataIterator dataItems = currentProgram.getListing().getDefinedData(true);
        while (dataItems.hasNext()) {
            Data data = dataItems.next();
            Object value = data.getValue();
            if (!(value instanceof String)) {
                continue;
            }
            String text = (String)value;
            if (!text.toLowerCase(Locale.ROOT).contains(needle)) {
                continue;
            }
            output.append(data.getAddress())
                .append('\t')
                .append(text.replace("\r", "\\r").replace("\n", "\\n"))
                .append(System.lineSeparator());
            ++matchCount;
        }
        Files.writeString(new File(arguments[1]).toPath(), output.toString(),
            StandardCharsets.UTF_8);
        println("Found " + matchCount + " strings containing " + arguments[0]);
    }
}
