/*
 * Copyright 2011, Jeremy Stashewsky <jstash+node@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

var N = 1000;
var sys = require('sys');
var util = require('util');
var compress=require("../lib/compress");
var Stream = require('stream').Stream;
var EventEmitter = require('events').EventEmitter;
var assert = require('assert');

function delayed (fn) {
  setTimeout(fn, Math.floor(Math.random()*150.0)+10);
}

function gzipPair () {
  var gzip_header = Math.random() < 0.5 ? true : false;
  return [
    new compress.GzipStream(9,true,gzip_header), // level, want_buffer, gzip_header
    (Math.random() < 0.5)
      ? new compress.GunzipStream(true) // auto-detect gzip/deflate mode
      : new compress.GunzipStream(true,gzip_header) // explicit gzip/deflate mode
  ];
}
var bzipPair = !compress.bzipSupport ? gzipPair : function () {
  return [
    new compress.BzipStream(1,0,true), // blockSize100k, workFactor, want_buffer
    new compress.BunzipStream(true,true)  // small, want_buffer
  ];
}

function startRound (n) {
  var pair = (n%2) ? gzipPair() : bzipPair();
  var compressor = pair[0],
      decompressor = pair[1];
  compressor.setEncoding('binary');
  decompressor.setEncoding('binary');

  var prefix = "For round "+n+", here is chunk number ";
  var expect = prefix+"1\n"+prefix+"2\n"+prefix+"3\n"+prefix+"4\n"+prefix+"5\n";
  var result = '';
  var sink = new EventEmitter();
  sink.writable = true;
  sink.write = function (chunk) { result += chunk.toString('utf8') };
  sink.end = function () {
    assert.equal(result,expect);
    sys.puts("ok "+n);
  };

  var er = function (err) {
    assert.ok(false, "round "+n+" error: "+err);
  };
  sink.on('error', er);
  compressor.on('error', er);
  decompressor.on('error', er);

  Stream.prototype.pipe.call(compressor, decompressor);
  Stream.prototype.pipe.call(decompressor, sink);

  process.nextTick(function() {
    //sys.puts("-- round "+n+" starting");
    compressor.write(new Buffer(prefix+"1\n"));
    compressor.write(new Buffer(prefix+"2\n"));
    delayed(function() {
      // previously a bug would execute deflating these two buffers in parallel
      // and another bug would accidentally drop the "3" chunk
      compressor.write(new Buffer(prefix+"3\n"));
      compressor.write(new Buffer(prefix+"4\n"));
      delayed(function() {
        compressor.write(new Buffer(prefix+"5\n"));
        compressor.end();
      });
    });
  });
}

do { startRound(N) } while (--N)
