//
//   Convert Unicode Confusable data files to ICU data format.
/*
***************************************************************************
* Copyright (C) 2008, International Business Machines Corporation,
* Google, and others. All Rights Reserved.
***************************************************************************
*/

//  This tool reads the Confusbles data files provided by the Unicode Consortium,
//  and writes a binary data file to be used directly by ICU4C.
//
//  The source data files originate here:
//
//       http://www.unicode.org/reports/tr39/data/intentional.txt
//       http://www.unicode.org/reports/tr39/data/confusables.txt
//       http://www.unicode.org/reports/tr39/data/confusablesWholeScript.txt
//
//  The format of the output binary data is described in in ICU4C,
//    in the header file common/spoofim.h

//  Output Data Format
//
//    struct MappedData {           // Standard ICU4C data headers.
//      struct UDataInfo  { ...}
//    }
//
//    struct SpoofDataHeader
//        int32_t       fMagic;   (0x8345fdef)
//        int32_t       fLength;   // sizeof(SpoofDataHeader)

//        int32_t       byte offset to String Lengths table
//        int32_t       number of entries in lengths table. (2 x 16 bits each)
//
//        int32_t       byte offset to Keys table
//        int32_t       number of entries in keys table  (32 bits each)
//
//        int32_t       byte offset to String Indexes table
//        int32_t       number of entries in String Indexes table (16 bits each)
//                      (number of entries must be same as in Keys table
//
//        int32_t       byte offset of String table
//        int32_t       length of string table (in 16 bit UChars)
//
//        int32_t       unused[6];   // Padding, Room for Expansion
//     }   // end of SpoofDataHeader
//
//     struct {
//          uint16_t   index in string table of last string with this length
//          uint16_t   this length
//        }  StringLengthsTable[n]
//
//        uint32_t keysTable[n]
//             // bits 0-23    a code point value
//             // bits 24-31   flags
//             //    24:  1 if entry applies to SL table
//             //    25:  1 if entry applies to SA table
//             //    26:  1 if entry applies to ML table
//             //    27:  1 if entry applies to MA table
//             //    28:  1 if there are multiple entries for this code point.
//             // Keytable is sorted in ascending code point order.
//
//        uint16_t stringIndexTable[n]
//             //  parallel table to keyTable, above.
//
//        UChar stringTable[o]
//             //  All replacement chars or strings, all concatenated together.
//
//        TBD:  possibly add a Latin1 fast table, to avoid searching the keytable.


import java.io.OutputStream;
import java.io.FileOutputStream;
import java.io.DataOutputStream;
import java.io.BufferedOutputStream;
import java.io.BufferedReader;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.IOException;
import java.net.URL;
import java.util.Map;
import java.util.TreeMap;
import java.util.TreeSet;
import java.util.HashSet;
import java.util.HashMap;
import java.util.Comparator;
import java.util.Vector;
import java.util.regex.Pattern;
import java.util.regex.Matcher;



public class Confusable {


	TreeMap<Integer, String>  SLMap = new TreeMap<Integer, String>();
	TreeMap<Integer, String>  SAMap = new TreeMap<Integer, String>();
	TreeMap<Integer, String>  MLMap = new TreeMap<Integer, String>();
	TreeMap<Integer, String>  MAMap = new TreeMap<Integer, String>();
	
	TreeSet<Integer>          allKeys    = new TreeSet<Integer>();
	TreeSet<String>           allStrings = new TreeSet<String>(new StringSorter());
	HashMap<String, Integer>  stringTableMap = new HashMap<String, Integer>();
	String                    stringTable;
	
	//
	//  Comparator for sorting strings in the order we need for the string table,
	//             sorted first by length.  After length, the order is unimportant
	//             but some ordering is needed.
	static class StringSorter implements Comparator<String> {
	    public int compare(String s1, String s2) {
	        if (s1.length() < s2.length()) {
	            return -1;
	        } else if (s1.length() > s2.length()) {
	            return +1;
	        } else return s1.compareTo(s2);
	    }
	}
	
    static int SL_BIT   = 1 << 24;
    static int SA_BIT   = 1 << 25;
    static int ML_BIT   = 1 << 26;
    static int MA_BIT   = 1 << 27;
    static int DUPL_BIT = 1 << 28;
    
	static class KeyTableEntry {
	    int     keyChar;
	    int     tableTypes;    // SL_BIT, etc.
	    String  string;        // The value (replacement) string
	    int     strIndex;      // String table index of the value string.
	}
    Vector<KeyTableEntry>     keyTable   = new Vector<KeyTableEntry>();
	
	//
	//  The String Length Table.
	//     
	static class StringLengthTableEntry  {
	    int     strLength;
	    int     lastIndexWithLength;
	    StringLengthTableEntry(int len, int lastIndex) {
	        strLength =  len;
	        lastIndexWithLength = lastIndex;
	    }
	}
	Vector<StringLengthTableEntry>  StringLengthTable = new Vector<StringLengthTableEntry>();
	
	//-----------------------------------------------------------------------------------------
	//
	//  buildTables  -  Build up a logical form of the tables that will eventually be written
	//                  out.
	//
	//------------------------------------------------------------------------------------------
	void buildTables() {
	    // Build up the String Table, and
	    //   the map from Strings to indexes in the String Table, and
	    //   the run-time map from string table indexes to string lengths.
	    //   The "allStrings" set is already sorted in the order we need,
	    //     by string length first.
	    StringBuilder sbuilder = new StringBuilder();
	    int previousStringLength = 1;
	    int previousStringIndex  = 0;
	    for (String s: allStrings) {
            if (s.length() > previousStringLength) {
                // We have just encountered the first string of the next bigger length.
                //  Add an entry to the runtime table for this transition.
                StringLengthTable.add(new StringLengthTableEntry(previousStringLength, previousStringIndex));
            }
            // Put the string into the run-time string table, and into our
            //   build time map from string to run-time table index.
            previousStringIndex = sbuilder.length();
            previousStringLength = s.length();
	        stringTableMap.put(s, sbuilder.length());
	        sbuilder.append(s);
	    }
	    stringTable = sbuilder.toString();
	    
	    //  Build up the maps themselves, consisting of the key table
	    //    and the parallel string index table
	    //
	    for (int keyChar: allKeys) {
	        addToKeyTable(keyChar, SLMap, SL_BIT);
            addToKeyTable(keyChar, SAMap, SA_BIT);
            addToKeyTable(keyChar, MLMap, ML_BIT);
            addToKeyTable(keyChar, MAMap, MA_BIT);
	    }
	}
	
	//------------------------------------------------------------------------------------
	//
	//  writeTables  -  Write the tables to the output file in the flattened binary
	//                  format needed for ICU4C.
	//
	//------------------------------------------------------------------------------------
	void writeTables() {
	    try {
	    OutputStream os = new FileOutputStream("confusables.cfu");
	    DataOutputStream dos = new DataOutputStream(new BufferedOutputStream(os));
	    
	    writeICUDataHeader(dos);
	    
	    // Start of SpoofDataHeader
	    dos.writeInt(0x8345fdef);
	    int   ICUHeaderSize = dos.size();   // bytes
	    int   CFUHeaderSize = 64;           // bytes.
	    
	    int   stringLengthsOffset = CFUHeaderSize;
	    int   stringLengthsSize   = StringLengthTable.size() * 4;
	    
	    int   keysTableOffset     = stringLengthsOffset + stringLengthsSize;
	    int   keysTableSize       = keyTable.size() * 4;
	    
	    int   stringsIndexOffset  = keysTableOffset + keysTableSize;
	    int   stringsIndexSize    = keyTable.size() * 2;
	    if (stringsIndexSize % 4 != 0) {
	        stringsIndexSize += 2;	        
	    }
	    
	    int   stringsOffset       = stringsIndexOffset + stringsIndexSize;
	    int   stringsSize         = stringTable.length() * 2;
        if (stringsSize % 4 != 0) {
            stringsSize += 2;          
        }
        int totalSize = ICUHeaderSize + stringsOffset + stringsSize;
        dos.writeInt(totalSize);
	    
	    dos.writeInt(stringLengthsOffset);
        dos.writeInt(StringLengthTable.size());
        
        dos.writeInt(keysTableOffset);
        dos.writeInt(keyTable.size());
        
        dos.writeInt(stringsIndexOffset);
        dos.writeInt(keyTable.size());
        
        dos.writeInt(stringsOffset);
        dos.writeInt(stringTable.length());
	    
	    for (StringLengthTableEntry el: StringLengthTable) {
	        dos.writeShort(el.lastIndexWithLength);
	        dos.writeShort(el.strLength);
	    }
	    
	    //
	    //  Write the keys array.
	    //
	    assert dos.size()==keysTableOffset + ICUHeaderSize;
	    for (KeyTableEntry el: keyTable) {
	        int tableContents = el.keyChar | el.tableTypes;
	        dos.writeInt(tableContents);
	    }
	    
	    //
	    //  Write the array of indexes into the string table
	    //    Parallels (is same length) as the key array, above.
	    //
	    assert dos.size() == stringsIndexOffset + ICUHeaderSize;
	    for (KeyTableEntry el: keyTable) {
	        dos.writeShort(el.strIndex);
	    }
	    if ((keyTable.size() & 1) != 0) {
	        // Pad output file up to a 32 bit boundary if table length is odd
	        dos.writeShort(0);
	    }
	    
	    //
	    // Write the string table, all strings concatenated together.
	    //
	    assert(dos.size() == stringsOffset + ICUHeaderSize);
	    dos.writeChars(stringTable);
	    
	    
        dos.flush();
	    } catch (Exception e) {
	        System.out.println(e.toString());
	    }
	    
	}
	
	//----------------------------------------------------------------------------------------
	//
	//  addToKeyTable    The Key Table holds a merged combination of the four
	//                   individual confusable tables as defined by Unicode.
	//                   This function adds a single mapping from one of the original tables 
	//                   to the combined table
	//
	//----------------------------------------------------------------------------------------
	void addToKeyTable(int keyChar, Map<Integer, String> map, int mapBit) {
	    String val = map.get(keyChar);
	    if (val == null) {
	        // The original table does not contain a mapping for this key char.
	        //   We don't need to do anything.
	        return;
	    }
	    
	    // Check whether the KeyTable already has this key -> value mapping.
	    //  If it does, just add the bit for the current table to the existing mapping.
	    for (int i=keyTable.size()-1; i>=0; i--) {
	        KeyTableEntry el = keyTable.get(i);
	        if (el.keyChar != keyChar) {
	            break;
	        }
	        if (el.string.equals(val)) {
	            // The combined table does have the desired mapping.
	            // Set the flag bit in it saying that it applies to the current input table.
	            el.tableTypes |= mapBit;
	            return;
	        }
	    }
	    
	    // The combined keytable does not have the needed mapping.  Append a new entry.
	    KeyTableEntry newEl = new KeyTableEntry();
	    newEl.keyChar = keyChar;
	    newEl.tableTypes = mapBit;
	    newEl.string = val;
	    newEl.strIndex = stringTableMap.get(val);
	    keyTable.add(newEl);
	   
	    // If the newly added keytable element is a duplicate - if it has the
	    //    same key char as the preceding element - set the duplicate bit
	    //    on both.
	    if (keyTable.size()>=2) {
	        KeyTableEntry previousEl = keyTable.get(keyTable.size()-2);
	        if (previousEl.keyChar == keyChar) {
	            previousEl.tableTypes    |= DUPL_BIT;
	            newEl.tableTypes         |= DUPL_BIT;
	        }
	    }
	}

    public static void main(String[] args) {
        Confusable This = new Confusable();
        This.Main(args);
    }
        
	public void Main(String[] args) {
		try {
		    //  TODO:  command line parameters
		    
            BufferedReader f;
			InputStream is;
			// is = new FileInputStream("fileName");
			is = new URL("http://www.unicode.org/reports/tr39/data/confusables.txt").openStream();
			InputStreamReader r = new InputStreamReader(is, "utf-8");
			f = new BufferedReader(r);	

			// Regular Expression to parse a line from Confusables.txt
			//   Capture Group 1:  the source char
			//   Capture Group 2:  the replacement chars
			//   Capture Group 3:  the table type, SL, SA, ML, or MA
			Matcher parseLine = Pattern.compile("([0-9A-F]+)\\s+;([^;]+);\\s+(..)").matcher("");
			
			// Regexp for parsing a hex number out of a space-separated list of them.
			//   Capture group 1 gets the number, with spaces removed.
			Matcher parseHex = Pattern.compile("\\s*([0-9A-F]+)").matcher("");
			
			// Read the confusables.txt file, build up the four individual mapping tables
			//
			String s;
			int lineCount = 0;
			int mapLineCount = 0;			
			while ((s=f.readLine()) != null) {
				lineCount++;
				if (parseLine.reset(s).lookingAt()) {
					// We have a line with a confusable mapping.
					//   (Comments are filtered.)
					mapLineCount++;
					
					Map<Integer, String>  table;
					if (parseLine.group(3).equals("SL")) {
						table = SLMap;
					} else if (parseLine.group(3).equals("SA")) {
						table = SAMap;
					} else if (parseLine.group(3).equals("ML")) {
						table = MLMap;
					} else if (parseLine.group(3).equals("MA")) {
						table = MAMap;
					} else {
						System.out.println("Unrecongized table type in input line\n" + s);
						continue;
					}
					
					int keyChar = Integer.parseInt(parseLine.group(1), 16);	
					if (table.get(keyChar) != null) {
						System.out.println("Error, duplicate key.");
					}
					StringBuilder replacementString = new StringBuilder();
					parseHex.reset(parseLine.group(2));
					while (parseHex.find()) {
						int rightChar = Integer.parseInt(parseHex.group(1), 16);
						replacementString.appendCodePoint(rightChar);
					}
					table.put(keyChar, replacementString.toString());
					allKeys.add(keyChar);
					allStrings.add(replacementString.toString());
				}
			}
			
			//
		    // Build the combined confusable tables, that fairly closely match
			//  the structure of the runtime data.
			//
			buildTables();
			
			//
			// Write out the binary runtime form of the tables
			//
			writeTables();
			
			System.out.println("lineCount = " + lineCount);
			System.out.println("mapLineCount = " + mapLineCount);
			printTableSummary(SLMap, "SL");
			printTableSummary(SAMap, "SA");
			printTableSummary(MLMap, "ML");
			printTableSummary(MAMap, "MA");

			checkConsistentEntries(SLMap, MAMap, "SL", "MA");
			checkConsistentEntries(SLMap, MLMap, "SL", "ML");
			checkConsistentEntries(SAMap, MAMap, "SA", "MA");

		}
		catch (Exception e) {
			System.out.println("Exception : " + e);
		}

	}
	
	public static void printTableSummary(Map<Integer, String> table, String name) {
		System.out.println("Stats for " + name);
		System.out.println("  num Keys: " + table.keySet().size());
		HashSet<String> values = new HashSet<String>();
		values.addAll(table.values());
		System.out.println("  num Values " + values.size());
		
		HashSet<String> nonBMPValues = new HashSet<String>();
		HashSet<String> BMPValuesEx = new HashSet<String>();
		System.out.print("  pure BMP entries: ");
		int pureBMPCount = 0;
		int maxValLength = 0;
		for (Integer cp: table.keySet()) {
			String tableValue = table.get(cp);
			if (cp <= 0xffff && tableValue.length() == 1) {
				pureBMPCount++;
			}
			if (tableValue.length() > 1) {
				nonBMPValues.add(tableValue);
			}
			if (tableValue.length() == 1 && cp>=0x10000) {
				BMPValuesEx.add(table.get(cp));
			}
			if (tableValue.length() > maxValLength) {
				maxValLength = tableValue.length();
			}
			
		}
		System.out.println(pureBMPCount);
		System.out.println("  Non-BMP values: " + nonBMPValues.size());
		System.out.println("  Supplementary keys with single BMP values: " + BMPValuesEx.size());
		System.out.println("  Maximum string length = " + maxValLength);
		
		int stringTableSize = 0;
		for (String value: nonBMPValues) {
			stringTableSize += value.length();
		}
		for (String value: BMPValuesEx) {
			stringTableSize += value.length();
		}
		System.out.println("  String table size: " + stringTableSize);
		
		System.out.println("\n");
		
	}
	
	public static void checkConsistentEntries(Map<Integer, String> tableA, Map<Integer, String> tableB,
			String NameA, String NameB) {	
		int total = 0;
		int conflicts = 0;
		HashSet <Integer> allKeys = new HashSet<Integer>();
		allKeys.addAll(tableA.keySet());
		allKeys.addAll(tableB.keySet());
		total = allKeys.size();
		for (Integer key: allKeys) {
			String valueA = tableA.get(key);
			String valueB = tableB.get(key);
			if (valueA != null && valueB != null && !valueA.equals(valueB)) {
				conflicts++;
			}
		}
		System.out.println("  " + NameA + ", " + NameB + ":  keys: " + total + "  conflicts: " + conflicts);
		
	}
	
	//
	//   writeICUDataHeader   Write out a Data Header for the Confusable data.
	//                        Definition are in the ICU4C header file udata.h
    //                           and in ucmndata.h
	void writeICUDataHeader(DataOutputStream dos) throws IOException {
	    //
	    // struct MappedData
	    //    uint16_t sizeof(DataHeader)    /* inluding the embedded UDataInfo, + anything following */
	    //    uint8_t  magic1 =  0xda            
	    //    uint8_t  magic2 =  0x27
	    //
	    // typedef struct UDataInfo {
	    //     /** sizeof(UDataInfo)  = 0x14
	    //      *  @stable ICU 2.0 */
	    //       uint16_t size;

	    //     /** unused, set to 0 
	    //      *  @stable ICU 2.0*/
	    //     uint16_t reservedWord;

	    //     /* platform data properties */
	    //     /** 0 for little-endian machine, 1 for big-endian
	    //      *  @stable ICU 2.0 */
	    //     uint8_t isBigEndian;

	    //    /** see U_CHARSET_FAMILY values in utypes.h 
	    //      *  @stable ICU 2.0*/
	    //     uint8_t charsetFamily;

	    //     /** sizeof(UChar), one of { 1, 2, 4 } 
	    //      *  @stable ICU 2.0*/
	    //     uint8_t sizeofUChar;

	    //     /** unused, set to 0 
	    //      *  @stable ICU 2.0*/
	    //     uint8_t reservedByte;

	    //     /** data format identifier 
	    //      *  @stable ICU 2.0*/
	    //     uint8_t dataFormat[4];

	    //     /** versions: [0] major [1] minor [2] milli [3] micro 
	    //      *  @stable ICU 2.0*/
	    //      uint8_t formatVersion[4];

	    //     /** versions: [0] major [1] minor [2] milli [3] micro 
	    //      *  @stable ICU 2.0*/
	    //      uint8_t dataVersion[4];
	    //  } UDataInfo;

	    int pos = dos.size();
	    int headerSize = 0x80;
	    
	                            // struct MappedData
	    dos.writeShort(headerSize);   // size of header
	    dos.writeByte(0xda);    //  magic1
	    dos.writeByte(0x27);    //  magic2
	    
	                            // struct UDataInfo
	    dos.writeShort(0x14);   // sizeof(UDataInfo)
	    dos.writeShort(0);      // reservedWord
	    dos.writeByte(1);       // isBigEndian.  Java is always big endian.
	    dos.writeByte(0);       // Char set family = ASCII
	    dos.writeByte(2);       // UChar is 2 bytes
	    dos.writeByte(0);       // reservedByte
	    
	    dos.writeByte('c');     // Data type = "cfu "
	    dos.writeByte('f');
	    dos.writeByte('u');
	    dos.writeByte(' ');
	    
	    dos.writeByte(1);       // Data version = 1.0.0.0
	    dos.writeByte(0);
	    dos.writeByte(0);
	    dos.writeByte(0);
	    
	    dos.writeByte(5);       // Unicode Version = 5.1.0.0
	    dos.writeByte(1);
        dos.writeByte(0);
        dos.writeByte(0);
                                // End of UDataInfo
        
        // Pad out to the full header size.
        while (dos.size()-pos < headerSize) {
            dos.writeByte(0);
        }
	    assert dos.size() == headerSize;
	    
	}
}

