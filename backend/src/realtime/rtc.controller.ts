import {
  Body,
  Controller,
  Get,
  HttpCode,
  HttpStatus,
  Param,
  ParseUUIDPipe,
  Post,
  Put,
  UseGuards,
} from '@nestjs/common';
import { CurrentDevice } from '../auth/current-device.decorator';
import { DeviceAuthGuard } from '../auth/device-auth.guard';
import type { DeviceRecord } from '../devices/device.entity';
import {
  CloseDataChannelsDto,
  CloseTracksDto,
  EstablishDataChannelsDto,
  NewDataChannelsDto,
  NewTracksDto,
  RenegotiateDto,
} from './dto/rtc.dto';
import { RtcService } from './rtc.service';

/**
 * Authenticated proxy to the Cloudflare Realtime SFU. Each route operates on
 * the caller's own SFU session within the given connection.
 */
@Controller()
@UseGuards(DeviceAuthGuard)
export class RtcController {
  constructor(private readonly rtc: RtcService) {}

  /** Short-lived STUN/TURN configuration for RTCPeerConnection. */
  @Get('rtc/ice-servers')
  iceServers() {
    return this.rtc.iceServers();
  }

  @Post('connections/:connectionId/rtc/sessions')
  createSession(
    @CurrentDevice() device: DeviceRecord,
    @Param('connectionId', ParseUUIDPipe) connectionId: string,
  ) {
    return this.rtc.createSession(device, connectionId);
  }

  @Post('connections/:connectionId/rtc/tracks')
  @HttpCode(HttpStatus.OK)
  newTracks(
    @CurrentDevice() device: DeviceRecord,
    @Param('connectionId', ParseUUIDPipe) connectionId: string,
    @Body() dto: NewTracksDto,
  ) {
    return this.rtc.newTracks(device, connectionId, dto);
  }

  @Put('connections/:connectionId/rtc/renegotiate')
  renegotiate(
    @CurrentDevice() device: DeviceRecord,
    @Param('connectionId', ParseUUIDPipe) connectionId: string,
    @Body() dto: RenegotiateDto,
  ) {
    return this.rtc.renegotiate(device, connectionId, dto);
  }

  @Put('connections/:connectionId/rtc/tracks/close')
  closeTracks(
    @CurrentDevice() device: DeviceRecord,
    @Param('connectionId', ParseUUIDPipe) connectionId: string,
    @Body() dto: CloseTracksDto,
  ) {
    return this.rtc.closeTracks(device, connectionId, dto);
  }

  @Post('connections/:connectionId/rtc/datachannels/establish')
  @HttpCode(HttpStatus.OK)
  establishDataChannels(
    @CurrentDevice() device: DeviceRecord,
    @Param('connectionId', ParseUUIDPipe) connectionId: string,
    @Body() dto: EstablishDataChannelsDto,
  ) {
    return this.rtc.establishDataChannels(device, connectionId, dto);
  }

  @Post('connections/:connectionId/rtc/datachannels')
  @HttpCode(HttpStatus.OK)
  newDataChannels(
    @CurrentDevice() device: DeviceRecord,
    @Param('connectionId', ParseUUIDPipe) connectionId: string,
    @Body() dto: NewDataChannelsDto,
  ) {
    return this.rtc.newDataChannels(device, connectionId, dto);
  }

  @Put('connections/:connectionId/rtc/datachannels/close')
  closeDataChannels(
    @CurrentDevice() device: DeviceRecord,
    @Param('connectionId', ParseUUIDPipe) connectionId: string,
    @Body() dto: CloseDataChannelsDto,
  ) {
    return this.rtc.closeDataChannels(device, connectionId, dto);
  }
}
