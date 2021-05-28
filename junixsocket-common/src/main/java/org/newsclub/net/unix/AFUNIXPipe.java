/*
 * junixsocket
 *
 * Copyright 2009-2021 Christian Kohlschütter
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
package org.newsclub.net.unix;

import java.io.FileDescriptor;
import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.channels.Pipe;

/**
 * A {@link Pipe}, natively implemented. Mostly to support {@link AFUNIXSelector}.
 * 
 * @author Christian Kohlschütter
 */
public final class AFUNIXPipe extends Pipe {
  private final AFUNIXCore sourceCore;
  private final AFUNIXCore sinkCore;
  private final SourceChannel sourceChannel;
  private final SinkChannel sinkChannel;

  AFUNIXPipe(AFUNIXSelectorProvider provider) throws IOException {
    super();

    this.sourceCore = new AFUNIXCore(this);
    this.sinkCore = new AFUNIXCore(this);

    NativeUnixSocket.initPipe(sourceCore.fd, sinkCore.fd);

    this.sourceChannel = new SourceChannel(provider) {

      @Override
      public long read(ByteBuffer[] dsts, int offset, int length) throws IOException {
        if (length == 0) {
          return 0;
        }
        return read(dsts[offset]);
      }

      @Override
      public long read(ByteBuffer[] dsts) throws IOException {
        return read(dsts, 0, dsts.length);
      }

      @Override
      public int read(ByteBuffer dst) throws IOException {
        return sourceCore.read(dst, null, NativeUnixSocket.OPT_NON_SOCKET);
      }

      @Override
      protected void implConfigureBlocking(boolean block) throws IOException {
        sourceCore.implConfigureBlocking(block);
      }

      @Override
      protected void implCloseSelectableChannel() throws IOException {
        sourceCore.close();
      }
    };

    this.sinkChannel = new SinkChannel(provider) {

      @Override
      public long write(ByteBuffer[] srcs, int offset, int length) throws IOException {
        if (length == 0) {
          return 0;
        }
        return write(srcs[offset]);
      }

      @Override
      public long write(ByteBuffer[] srcs) throws IOException {
        return write(srcs, 0, srcs.length);
      }

      @Override
      public int write(ByteBuffer src) throws IOException {
        return sinkCore.write(src, null, NativeUnixSocket.OPT_NON_SOCKET);
      }

      @Override
      protected void implConfigureBlocking(boolean block) throws IOException {
        sinkCore.implConfigureBlocking(block);
      }

      @Override
      protected void implCloseSelectableChannel() throws IOException {
        sinkCore.close();
      }
    };
  }

  @Override
  public SourceChannel source() {
    return sourceChannel;
  }

  @Override
  public SinkChannel sink() {
    return sinkChannel;
  }

  FileDescriptor sourceFD() {
    return sourceCore.fd;
  }

  FileDescriptor sinkFD() {
    return sinkCore.fd;
  }
}
