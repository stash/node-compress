var compress=require("./compress");
var sys=require("sys");
var posix=require("fs");

// Read in our test file
var data=posix.readFileSync("bzfiletest.js", encoding="binary");
sys.puts("Got : "+data.length);

// Set output file
var fd = posix.openSync("bzfiletest.js.bz2",
    process.O_WRONLY | process.O_TRUNC | process.O_CREAT, 0644);
sys.puts("Openned file");

// Create bzip stream
var bzip=new compress.Bzip;
bzip.init();

// Pump data to be compressed
gzdata=bzip.deflate(data, "binary");  // Do this as many times as required
sys.puts("Compressed size : "+gzdata.length);
posix.writeSync(fd, gzdata, encoding="binary");

// Get the last bit
gzlast=bzip.end();
sys.puts("Last bit : "+gzlast.length);
posix.writeSync(fd, gzlast, encoding="binary");
posix.closeSync(fd);
sys.puts("File closed");

// See if we can uncompress it ok
var bunzip=new compress.Bunzip;
bunzip.init();
var testdata = posix.readFileSync("bzfiletest.js.bz2", encoding="binary");
sys.puts("Test opened : "+testdata.length);
var source = bunzip.inflate(testdata, "binary");
sys.puts(source.length);
sys.puts(source);
bunzip.end();






