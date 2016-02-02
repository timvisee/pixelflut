package de.paws.pixelwar;

import io.netty.buffer.ByteBuf;
import io.netty.channel.ChannelHandlerContext;
import io.netty.channel.ChannelInboundHandlerAdapter;

import java.nio.charset.Charset;

public class PixelDecoder extends ChannelInboundHandlerAdapter {

	static final byte space = ' ';
	static final Charset utf8 = Charset.forName("utf8");

	@Override
	public void channelRead(final ChannelHandlerContext ctx, final Object msg)
			throws Exception {
		if (msg instanceof ByteBuf) {
			final ByteBuf bytes = (ByteBuf) msg;
			int i;
			while ((i = bytes.bytesBefore(space)) >= 0) {
				final String chunk = bytes.readBytes(i).toString(utf8);
			}
		} else {
			super.channelRead(ctx, msg);
		}
	}
}
