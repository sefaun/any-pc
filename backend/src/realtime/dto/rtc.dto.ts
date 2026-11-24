import { Type } from 'class-transformer';
import {
  ArrayMaxSize,
  ArrayMinSize,
  IsArray,
  IsBoolean,
  IsIn,
  IsInt,
  IsOptional,
  IsString,
  Length,
  Matches,
  Max,
  MaxLength,
  Min,
  ValidateNested,
} from 'class-validator';

const RESOURCE_NAME = /^[A-Za-z0-9_-]{1,64}$/;
const MAX_SDP_LENGTH = 128 * 1024;
const MAX_BATCH = 8;

export class SessionDescriptionDto {
  @IsIn(['offer', 'answer'])
  type: 'offer' | 'answer';

  @IsString()
  @Length(1, MAX_SDP_LENGTH)
  sdp: string;
}

export class OfferDescriptionDto extends SessionDescriptionDto {
  @IsIn(['offer'])
  declare type: 'offer';
}

export class AnswerDescriptionDto extends SessionDescriptionDto {
  @IsIn(['answer'])
  declare type: 'answer';
}

/**
 * A track entry. For `remote` entries the server fills in the peer's
 * session id; clients never address arbitrary SFU sessions.
 */
export class TrackDto {
  @IsIn(['local', 'remote'])
  location: 'local' | 'remote';

  @IsOptional()
  @IsString()
  @MaxLength(32)
  mid?: string;

  @Matches(RESOURCE_NAME)
  trackName: string;

  @IsOptional()
  @IsIn(['audio', 'video'])
  kind?: 'audio' | 'video';
}

export class NewTracksDto {
  @IsOptional()
  @ValidateNested()
  @Type(() => OfferDescriptionDto)
  sessionDescription?: OfferDescriptionDto;

  @IsArray()
  @ArrayMinSize(1)
  @ArrayMaxSize(MAX_BATCH)
  @ValidateNested({ each: true })
  @Type(() => TrackDto)
  tracks: TrackDto[];
}

export class RenegotiateDto {
  @ValidateNested()
  @Type(() => AnswerDescriptionDto)
  sessionDescription: AnswerDescriptionDto;
}

export class CloseTrackDto {
  @IsString()
  @Length(1, 32)
  mid: string;
}

export class CloseTracksDto {
  @IsArray()
  @ArrayMinSize(1)
  @ArrayMaxSize(MAX_BATCH)
  @ValidateNested({ each: true })
  @Type(() => CloseTrackDto)
  tracks: CloseTrackDto[];

  @IsOptional()
  @ValidateNested()
  @Type(() => OfferDescriptionDto)
  sessionDescription?: OfferDescriptionDto;

  @IsBoolean()
  force: boolean;
}

export class EstablishDataChannelsDto {
  /** Optional endpoint offer; when omitted the SFU generates the offer. */
  @IsOptional()
  @ValidateNested()
  @Type(() => OfferDescriptionDto)
  sessionDescription?: OfferDescriptionDto;
}

export class DataChannelDto {
  @IsIn(['local', 'remote'])
  location: 'local' | 'remote';

  @Matches(RESOURCE_NAME)
  dataChannelName: string;

  @IsOptional()
  @IsBoolean()
  ordered?: boolean;

  @IsOptional()
  @IsInt()
  @Min(0)
  @Max(65535)
  maxRetransmits?: number;

  @IsOptional()
  @IsInt()
  @Min(0)
  @Max(65535)
  maxPacketLifeTime?: number;

  @IsOptional()
  @IsBoolean()
  waitForAck?: boolean;

  @IsOptional()
  @IsBoolean()
  canReply?: boolean;
}

export class NewDataChannelsDto {
  @IsArray()
  @ArrayMinSize(1)
  @ArrayMaxSize(MAX_BATCH)
  @ValidateNested({ each: true })
  @Type(() => DataChannelDto)
  dataChannels: DataChannelDto[];
}

export class CloseDataChannelDto {
  @IsInt()
  @Min(0)
  @Max(65535)
  id: number;
}

export class CloseDataChannelsDto {
  @IsArray()
  @ArrayMinSize(1)
  @ArrayMaxSize(MAX_BATCH)
  @ValidateNested({ each: true })
  @Type(() => CloseDataChannelDto)
  dataChannels: CloseDataChannelDto[];
}
