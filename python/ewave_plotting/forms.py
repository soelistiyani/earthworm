from flask_wtf import FlaskForm
from wtforms import StringField, SubmitField, BooleanField
from wtforms.validators import DataRequired, Length, Regexp, Optional


class DataRequestForm(FlaskForm):
    net = StringField('network', validators=[DataRequired(), Length(1, 2)])
    sta = StringField('station', validators=[DataRequired(), Length(1, 5)])
    loc = StringField('location', validators=[DataRequired(), Length(1, 2)])
    chan = StringField('channel', validators=[DataRequired(), Length(1, 3)])
    start = StringField('start time',
                        validators=[
                            DataRequired(),
                            Regexp(
                                regex=r'^(?:(?:19[0-9][0-9]|20[0-9][0-9])-(?:0[1-9]|11|12)-(?:[0-2][0-9]|30|31)T(?:[0-1][0-9]|2[0-3]):(?:[0-5][0-9]):(?:[0-5][0-9]))$')])
    dur = StringField('duration', validators=[DataRequired()])
    picktime = StringField('pick time',
                           validators=[
                               Optional(),
                               Regexp(regex=r'^$|^(?:(?:19[0-9][0-9]|20[0-9][0-9])-(?:0[1-9]|11|12)-(?:[0-2][0-9]|30|31)T(?:[0-1][0-9]|2[0-3]):(?:[0-5][0-9]):(?:[0-5][0-9]))$')])
    picklabel = StringField('pick label', validators=[Optional()])
    outputFormat = StringField('Output format',
                               validators=[
                                   Optional(),
                                   Regexp(
                                       regex=r'^(?:plot|png|mseed|miniseed)$')
                               ])
    displayMax = BooleanField('Display Max Value', validators=[Optional()])
    scaleFactor = StringField('Scale factor', validators=[Optional()])
    units = StringField('Output units', validators=[Optional()])
    submit = SubmitField('Submit')
