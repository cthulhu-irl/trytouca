// Copyright 2021 Touca, Inc. Subject to Apache-2.0 License.

import { CommonModule } from '@angular/common';
import { HttpClientModule } from '@angular/common/http';
import { NgModule } from '@angular/core';
import { FormsModule, ReactiveFormsModule } from '@angular/forms';
import { RouterModule } from '@angular/router';
import { FontAwesomeModule } from '@fortawesome/angular-fontawesome';
import { DialogModule } from '@ngneat/dialog';
import { ClipboardModule } from 'ngx-clipboard';
import { NgIconsModule } from '@ng-icons/core';
import {
  HeroBadgeCheck,
  HeroCheckCircle,
  HeroChevronDoubleLeft,
  HeroChevronDown,
  HeroClipboardCopy,
  HeroDotsVertical,
  HeroExclamationCircle,
  HeroEye,
  HeroEyeOff,
  HeroInformationCircle,
  HeroMail,
  HeroPlusSolid,
  HeroRefresh,
  HeroSpeakerphone,
  HeroTerminal,
  HeroUpload
} from '@ng-icons/heroicons';
import {
  FeatherChevronDown,
  FeatherList,
  FeatherSettings,
  FeatherUser,
  FeatherUsers
} from '@ng-icons/feather-icons';

import {
  AlertComponent,
  CheckboxComponent,
  FooterComponent,
  HeaderInsideComponent,
  HeaderOutsideComponent,
  NotificationComponent,
  ServerDownComponent
} from './components';
import { AutofocusDirective, DropdownDirective } from './directives';
import { DateAgoPipe, DateTimePipe } from './pipes';

@NgModule({
  imports: [
    ClipboardModule,
    CommonModule,
    DialogModule.forRoot(),
    FontAwesomeModule,
    FormsModule,
    HttpClientModule,
    NgIconsModule.withIcons({
      FeatherChevronDown,
      FeatherList,
      FeatherSettings,
      FeatherUser,
      FeatherUsers,
      HeroBadgeCheck,
      HeroCheckCircle,
      HeroChevronDoubleLeft,
      HeroChevronDown,
      HeroClipboardCopy,
      HeroDotsVertical,
      HeroExclamationCircle,
      HeroEye,
      HeroEyeOff,
      HeroInformationCircle,
      HeroMail,
      HeroPlusSolid,
      HeroRefresh,
      HeroSpeakerphone,
      HeroTerminal,
      HeroUpload
    }),
    ReactiveFormsModule,
    RouterModule
  ],
  declarations: [
    AlertComponent,
    AutofocusDirective,
    CheckboxComponent,
    DateAgoPipe,
    DateTimePipe,
    DropdownDirective,
    FooterComponent,
    HeaderInsideComponent,
    HeaderOutsideComponent,
    NotificationComponent,
    ServerDownComponent
  ],
  exports: [
    AlertComponent,
    AutofocusDirective,
    CheckboxComponent,
    ClipboardModule,
    CommonModule,
    DateAgoPipe,
    DateTimePipe,
    DialogModule,
    DropdownDirective,
    FontAwesomeModule,
    FooterComponent,
    FormsModule,
    HeaderInsideComponent,
    HeaderOutsideComponent,
    HttpClientModule,
    NgIconsModule,
    NotificationComponent,
    ReactiveFormsModule,
    ServerDownComponent
  ]
})
export class SharedModule {}
